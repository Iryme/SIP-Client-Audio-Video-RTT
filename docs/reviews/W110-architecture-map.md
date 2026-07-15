# W110 Architecture Map — SIP-Client-Audio-Video-RTT

Produced for the Task W110 full-project audit. Read-only inventory of
modules, ownership, threading, and the main application flows as they exist
on branch `audit/w110-full-project-code-review` (based on
`fix/w109a-rtt-renegotiation-and-rtp-port-collision`).

## 1. Top-level modules under `src/`

| Module | Responsibility | Key classes |
|---|---|---|
| **app** | Application bootstrap (QApplication subclass, stylesheet). | `Application` (owns `MainWindow*`) |
| **core** | Cross-cutting infrastructure: settings, logging, call history, diagnostics, contacts. | `AppSettings`, `Logger`, `CallHistoryStore`/`CallHistoryRecorder`/`CallHistoryListModel`, `ContactStore`, `DiagnosticsCollector`, `DiagnosticsTimelineService`/`Model`/`Exporter`, `DiagnosticsBundleExporter`, `PerfScope` |
| **emergency** | Emergency (911/112) call construction: PIDF-LO location, multipart INVITE bodies, dedicated state machine. | `EmergencyCallController`, `EmergencyCallStateMachine`, `EmergencyInviteBuilder`, `EmergencyCallAdapter`, `PidfLoBuilder`, `EmergencyMultipartBuilder`, `EmergencyLocationProvider`/`StaticLocationProvider` |
| **etsi** | ETSI real-time text codec (T.140-like) used under RTT. | `LmpeCodec`, `UnconfirmedLmpeCodec` |
| **gui** | Qt Widgets UI: main window, nav rail, per-feature panels/pages, dialogs. | `MainWindow`, `NavRail`, `CallPanel`, `VideoPanel`, `RttPanel`, `MsrpPage`, `XcapPage`, `PresencePage`, `MessagingDiagnosticsPage`, `DiagnosticsCenterPanel`, `SipLadderPage`, `IncomingCallDialog`, `MediaRequestDialog` |
| **media** | Audio/video device + PJSIP media plumbing, codec/quality management. | `AudioMediaManager`, `VideoMediaManager`, `MediaDeviceManager`, `MediaDeviceSelectionModel`, `VideoQualityManager`, `VideoPipelineMonitor`, `PjsipGdiRenderer`, `RtpStats` |
| **msrp** | MSRP (RFC 4975/4976) direct + relay transport, chunking, file transfer, diagnostics. | `MsrpSession`, `MsrpTransport`/`MsrpTcpTransport`/`MsrpTlsTransport`, `MsrpRelayClient`, `MsrpCallPreparationController`, `MsrpFrameParser`/`Serializer`, `MsrpChunkAssembler`, `MsrpMessageChunker`, `MsrpFileSelector`, `MsrpFileReceiver`, `MsrpSdpNegotiator` |
| **rtt** | RFC 4103 real-time text session over RTP. | `RttSession`, `RttConfig`, `RttTextUtils` |
| **security** | Credential storage abstraction (Windows Credential Manager / in-memory). | `CredentialStore`, `ICredentialBackend`, `WindowsCredentialBackend`, `MemoryCredentialBackend` |
| **sip** | Core SIP/pjsua2 wrapper: accounts, calls, registration, SIP MESSAGE/CPIM/IMDN/is-composing, Presence, XCAP, tracing/diagnostics. | `SipManager`, `SipAccount`, `SipCall`, `SipProfileManager`, `RegistrationStateMachine`, `CallStateMachine`, `CpimBuilder`/`Parser`, `ImdnGenerator`/`Parser`, `IsComposingGenerator`/`Parser`, `TypingIndicatorController`, `PresenceStore`, `XcapClient`, `MessageHistoryStore`, `MessagingEventStore`, `SipTraceLogger` |

## 2. Ownership model

- **SipManager** — Meyers singleton (`SipManager::instance()`), no parent; lives for app lifetime on the GUI/main thread. Owns `m_account` (raw `SipAccount*`, QObject-parented), `m_activeCall` (raw `SipCall*`, QObject-parented, destroyed via `destroyActiveCall()`), `m_msrpCallPreparation` (raw `MsrpCallPreparationController*`, lazily created, QObject-parented), and `m_rttSession` (value member `RttSession`).
- **SipAccount** — QObject-parented child of `SipManager`. Internally holds a nested `pj::Account` subclass whose `onRegState`/`onIncomingCall` callbacks fire on the PJSIP internal thread and are marshalled to Qt signals.
- **SipCall** — QObject-parented child of `SipManager` (3 creation sites: outbound, emergency, incoming). Internally wraps a pjsua2 `PjCall : public pj::Call` (raw, PImpl-owned). Owns `msrpSession` (raw `MsrpSession*`, manually managed in the PImpl dtor).
- **MsrpSession** — QObject; owns its transport via `std::unique_ptr<MsrpTransport>`, created as `MsrpTcpTransport`/`MsrpTlsTransport` on demand. Runs on the GUI thread using Qt async socket signals — no worker thread.
- **MsrpTransport** (abstract; `MsrpTcpTransport`/`MsrpTlsTransport`) — QObject, holds raw `QTcpSocket*`/`QTcpServer*`/`QTimer*` as Qt-parented children; documented as non-blocking.
- **MsrpRelayClient** — plain `new MsrpRelayClient()` (no QObject parent), holds `std::unique_ptr<MsrpTransport>` and exposes `takeTransport()` to hand ownership to the eventual `MsrpSession`. See finding W110-F001.
- **MsrpCallPreparationController** — QObject-parented child of `SipManager`; owns the relay client as `std::shared_ptr<MsrpRelayClient>`.
- **RttSession** — value member of `SipManager`; `enableForCall(SipCall*)` wires to the currently active call via `QPointer` without owning it.
- **File transfer classes** (`MsrpFileSelector`, `MsrpFileReceiver`) — stateless helpers, no ownership semantics.
- **MainWindow** — top-level widget owned by `Application`. Owns all panels as raw QObject/QWidget-parented children, lazily built via `ensurePage(int)`.

## 3. Threads

No `QThread` subclasses, `moveToThread`, `std::thread`, or `QtConcurrent` usage exists anywhere in `src/` — the entire Qt-facing application (GUI, MSRP transports, XCAP HTTP, diagnostics) runs on a single GUI/event-loop thread using async, signal-driven I/O.

The only other thread(s) are internal to pjsua2/pjlib, started implicitly by `pj::Endpoint::libStart()`. This worker thread invokes `onRegState`/`onIncomingCall`/`onCallState`/`onCallMediaState`/`onCallRxReinvite` callbacks. Every callback reviewed during this audit (`SipAccount.cpp`, `SipCall.cpp`) consistently guards the Qt-side continuation with `QPointer` + `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` before touching any `SipCall`/`SipManager` state — this is the correct, deliberate marshaling pattern and was verified, not assumed, by the lifecycle-review agent (no UI mutation was found directly inside a PJSIP callback).

## 4. Flow traces (file:function level)

- **App startup**: `main.cpp` → `applyEarlyCommandLineOverrides()` (config-dir/RTP port overrides) → `Application app(argc,argv)` constructs `MainWindow` → `app.exec()`.
- **Config load**: `AppSettings` backed by `QSettings`; profiles via `SipProfileManager`, both config-dir-overridable (Task W109A).
- **PJSIP init**: `SipManager::initialize()` → `initPjsip()` → `ep.libCreate()/libInit()/libStart()` → `PjsipTraceModule::install()`.
- **REGISTER**: `SipManager::registerActiveProfile()` → `new SipAccount(...)` → `PjAccount::onRegState` drives `RegistrationStateMachine`/`RegistrationRetryPolicy`.
- **Outbound call**: `SipManager::makeCall()`/`makeCall(opts)` → `prepareOutgoingCall()` → `new SipCall(...)` → `wireActiveCall()` → `dispatchMakeCall()` (resolves MSRP relay policy) → `SipCall::makeCallWithOptions()` → pjsua2 `Call::makeCall`.
- **Inbound call**: `PjAccount::onIncomingCall` → `EarlyCall` claims `user_data`, sends 180 → `SipManager::onAccountIncomingCall()` creates the real `SipCall` → `incomingCall(remoteUri)` → GUI dialog.
- **Answer/Reject/Hangup**: `SipManager::answerCall()/rejectCall()/hangupCall()` → `SipCall` → pjsua2.
- **Audio/Video**: `AudioMediaManager`/`VideoMediaManager` attach on `PjCall::onCallMediaState`; device selection via `MediaDeviceManager`.
- **RTT**: `SipManager::requestCallRtt()/acceptIncomingRtt()/rejectIncomingRtt()/sendRttText()` drive `m_rttSession` (`RttSession::enableForCall`), using `etsi::LmpeCodec` for RED.
- **SIP MESSAGE / CPIM / IMDN / is-composing**: `SipManager::sendSipMessage()` → `SipMessageComposer` → `CpimBuilder`/`ImdnGenerator`; inbound via `onAccountInstantMessageReceived()` → `MessageHistoryStore`; typing via `TypingIndicatorController` + `IsComposingGenerator`/`Parser`.
- **Presence**: `SipManager::subscribePresence()/unsubscribePresence()/...` drive pjsua2 `Buddy` subscriptions → `PresenceStore`; backoff via `PresenceResubscribePolicy` + per-entity timers.
- **XCAP**: `XcapClient` singleton issues async HTTP(S) via Qt network classes; `XcapRequestBuilder`/`XcapModels::buildUri` construct percent-encoded URIs; `XcapXmlValidator` validates bodies.
- **MSRP direct**: `SipCall::createMsrpSessionObject()` creates `MsrpSession`, selects transport based on negotiated `a=setup`/TLS, listens/connects; SDP via `MsrpSdpNegotiator`.
- **MSRP relay**: `SipManager::dispatchMakeCall()` → `buildMsrpRelayConfigFromSettings()` → `MsrpCallPreparationController` → `MsrpRelayClient` → allocation → `takeTransport()` hands the connected transport to `MsrpSession`.
- **File transfer**: `MsrpFileSelector` handles RFC 5547 `a=file-selector`; `MsrpFileReceiver` persists reassembled bytes after user picks a save path via `QFileDialog`.
- **Diagnostics**: `DiagnosticsCollector`/`DiagnosticsTimelineService` aggregate events from the per-subsystem diagnostics stores, surfaced in `DiagnosticsCenterPanel`.
- **Export**: `MainWindow::exportConfiguration()` → `DiagnosticsBundleExporter` (redaction) / `DiagnosticsTimelineExporter` (JSON/CSV).
- **Shutdown**: `MainWindow` close → `SipManager::shutdown()` → `destroyActiveCall()`/`destroyAccount()` → `shutdownPjsip()` → `shutdownComplete()`.

See [W110-findings.md](W110-findings.md) for the concrete defects found while verifying these flows.
