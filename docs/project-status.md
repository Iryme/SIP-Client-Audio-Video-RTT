# Project Status

Last updated: 2026-07-13
Current task count: 59 of N (Tasks 1–28.6 + Task 34–43 + Task W090–W105 complete)
Active branch: `feature/w105-msrp-advanced-hardening`

---

## Phase Summary

| Phase | Tasks | Status |
|---|---|---|
| Foundation (skeleton, GUI, settings, profiles, credentials, media devices, PJSIP build) | 1–10 | COMPLETE |
| Project Handoff 001 | 11 | COMPLETE |
| SIP Registration (SM, retry, expiry, profile switch) | 12–16 | COMPLETE — live validated |
| Call State Machine + Audio + Video + SIP Diagnostics | 17–20 | COMPLETE — live validated |
| Project Handoff 002 | 21 | COMPLETE |
| Real PJSIP Validation (registration + audio calls) | 22–23 | COMPLETE — live validated |
| Audio Device Selection (GUI) | 24 | COMPLETE — live validated |
| Video (codec, DirectShow, window embed, renderer) | 25–27 | COMPLETE — live validated |
| RTT RFC 4103 / T.140 (TX, RX, RED, GUI) | 28.1–28.6 | COMPLETE — live validated |
| Project Handoff 003 | 003 | COMPLETE |
| ETSI / NG112 Architecture Skeleton | 34 | COMPLETE — skeleton only, no real calls |
| Green build recovery (PJMEDIA_HAS_VIDEO fix) | 35 | COMPLETE — 20/20 CTest PASS |
| ETSI / NG112 Emergency SIP INVITE Builder | 36 | COMPLETE — declarative builder, no real call |
| ETSI / NG112 PIDF-LO Builder + Manual Location Model | 37 | COMPLETE — PIDF-LO builder, not yet sent over SIP |
| ETSI / NG112 Emergency SIP Integration Minimal | 38 | COMPLETE — SipCallOptions, header injection, EmergencyCallAdapter |
| ETSI / NG112 Multipart INVITE + PIDF-LO body | 39 | COMPLETE — EmergencyMultipartBuilder, PJSIP multipart injection, SDP preserved |
| ETSI / NG112 Emergency Protocol Validation | 40 | COMPLETE — 25/25 CTest PASS, live probe built, pre-INVITE chain validated, live run documented |
| ETSI / NG112 Live Emergency INVITE Capture | 41 | COMPLETE — live probe ran, INVITE confirmed on wire (404 expected), all NG112 headers validated |
| ETSI / NG112 Emergency GUI Wiring | 42 | COMPLETE — 112 button in CallPanel (hidden by default), EmergencyCallController wired to SipManager, StaticLocationProvider DEMO coords, 26/26 CTest PASS |
| ETSI / NG112 Manual PIDF-LO Generator + SIP Location Update | 43 | COMPLETE — manual lat/lon/unc UI, PIDF-LO preview, StaticLocationProvider.setLocation(), SipCall::sendLocationUpdate() via UPDATE, SipManager::sendEmergencyLocationUpdate(), 8 unit tests |

---

## Completed Tasks

| # | Task | Branch | Status |
|---|---|---|---|
| 1 | Project skeleton, GUI layout, documentation | feature/project-skeleton | COMPLETE |
| 2 | GUI layout specification | feature/gui-layout-spec | COMPLETE (separate lineage) |
| 3 | Diagnostics logger | feature/diagnostics-logger | COMPLETE (separate lineage, not merged) |
| 4 | Diagnostics GUI console | feature/diagnostics-gui-console | COMPLETE (separate lineage, not merged) |
| 5 | Persistent application settings | feature/persistent-settings | COMPLETE (separate lineage, not merged) |
| 6 | SIP profile model, persistence, GUI | feature/sip-profile-manager | COMPLETE |
| 7 | Secure credential storage (CredentialStore, Windows CM) | feature/secure-credential-storage | COMPLETE |
| 8 | SIP Profile Editor Dialog — Add/Edit/Delete | feature/sip-profile-editor | COMPLETE |
| 9 | Media device enumeration — MediaDeviceManager, MediaPanel | feature/media-device-enumeration | COMPLETE |
| 10 | PJSIP build integration — FindPJSIP.cmake, SipManager skeleton | feature/pjsip-build-integration | COMPLETE |
| 11 | Project Handoff 001 | feature/project-handoff-001 | COMPLETE |
| 12 | SIP Register / Unregister — PJSIP, callbacks, GUI status | feature/sip-registration | COMPLETE |
| 13 | Registration State Machine — 5-state SM, watchdog, guards | feature/registration-state-machine | COMPLETE |
| 14 | Registration Retry / Backoff — RegistrationRetryPolicy, exponential backoff | feature/registration-retry-backoff | COMPLETE |
| 15 | Registration Expiry & Auto Re-REGISTER — RegistrationRefreshConfig | feature/registration-expiry-refresh | COMPLETE |
| 16 | Profile Switch Sequencing — switchActiveProfile(), pending-switch guard | feature/profile-switch-sequencing | COMPLETE |
| 17 | SIP Call State Machine — CallStateMachine (9 states), SipCall, CallPanel | feature/call-state-machine | COMPLETE |
| 18 | Audio Media Integration — AudioMediaManager, mute, level meters, PJSIP bridge | feature/audio-media | COMPLETE |
| 19 | Video Media Integration — VideoMediaManager, VideoPanel, PJSIP bridge scaffold | feature/video-media | COMPLETE |
| 20 | SIP Diagnostics & Ladder — SipTraceLogger, SipLadderWidget, DiagnosticsPanel | feature/sip-diagnostics-ladder | COMPLETE |
| 21 | Project Handoff 002 | feature/project-handoff-002 | COMPLETE |
| 22 | Real PJSIP Integration Validation — registration + audio calls + fixes | feature/project-handoff-002 | COMPLETE — live validated |
| 23 | GUI Live Audio Call — dial row, URI normalization, hangup hardening | feature/project-handoff-002 | COMPLETE — live validated |
| 24 | Audio Device Selection — GUI combo → PJSIP AudDevManager | feature/project-handoff-002 | COMPLETE — live validated |
| 25 | Video codec + DirectShow + window embed (SetParent/MoveWindow) | feature/project-handoff-002 | COMPLETE — live validated |
| 26 | CodecManager — codec matrix, priorities, negotiation logging | feature/project-handoff-002 | COMPLETE |
| 27 | PjsipGdiRenderer — custom PJMEDIA GDI factory, preview stop fix, resize fix | feature/project-handoff-002 | COMPLETE — live validated |
| 28.1 | RTT foundation — RttSession, SDP m=text, PJSIP text stream detection | feature/project-handoff-002 | COMPLETE |
| 28.2 | RTT TX/RX — char-by-char delta TX, T.140 RX accumulation, RttPanel wiring | feature/project-handoff-002 | COMPLETE |
| 28.3 | RTT RED — redundancyLevel=2, AccountConfig, interop test doc | feature/project-handoff-002 | COMPLETE |
| 28.4 | RTT SDP negotiation probe validation | feature/project-handoff-002 | COMPLETE — live validated |
| 28.5 | RTT stream activation + audio + hangup cleanup validation | feature/project-handoff-002 | COMPLETE — live validated |
| 28.6 | RTT GUI end-to-end TX/RX validation (UIA automation) | feature/project-handoff-002 | COMPLETE — live validated |
| 34 | ETSI / NG112 Architecture Skeleton — emergency module, SM, profile, location interface | feature/project-handoff-002 | COMPLETE — skeleton only |
| 35 | Green Build Recovery — PJMEDIA_HAS_VIDEO + pjlib linkage for PJSIP test targets | feature/project-handoff-002 | COMPLETE — 20/20 CTest PASS |
| 36 | Emergency SIP INVITE Builder — EmergencyInviteBuilder, EmergencyInvite, validation | feature/project-handoff-002 | COMPLETE — declarative builder, no real call |
| 37 | PIDF-LO Builder + Manual Location — EmergencyLocation, PidfLoBuilder, StaticLocationProvider | feature/project-handoff-002 | COMPLETE — PIDF-LO builder, not yet sent over SIP |
| 38 | Emergency SIP Integration Minimal — SipCallOptions, makeCallWithOptions, EmergencyCallAdapter | feature/project-handoff-002 | COMPLETE — header injection path, normal call untouched |
| 39 | Emergency Multipart PIDF-LO Body — EmergencyMultipartBuilder, PJSIP multipart injection | feature/project-handoff-002 | COMPLETE — SDP preserved, PIDF-LO part attached, 24/24 CTest PASS |
| 40 | Emergency Protocol Validation — test_emergency_protocol_validation, live_emergency_call_probe | feature/project-handoff-002 | COMPLETE — 25/25 CTest PASS, pre-INVITE chain validated, live probe ready |
| 41 | Live Emergency INVITE Capture — live probe ran, headers confirmed, PIDF-LO confirmed, 404 from Kamailio (PSAP absent, expected) | feature/project-handoff-002 | COMPLETE — live validated |
| 42 | Emergency GUI Wiring — 112 button in CallPanel, EmergencyCallController → SipManager, StaticLocationProvider DEMO, confirm dialog TEST/LAB | feature/project-handoff-002 | COMPLETE — 26/26 CTest PASS |
| 43 | Manual PIDF-LO Generator + SIP Location Update — manual lat/lon/unc UI, PIDF-LO preview, SipCall::sendLocationUpdate() via SIP UPDATE, SipManager::sendEmergencyLocationUpdate() | feature/project-handoff-002 | COMPLETE — 26/26 CTest PASS (existing) + test_emergency_location_update 8 tests |
| W090 | Messaging and MSRP Diagnostics Foundation — MessagingDiagnosticsStore, CpimParser, ImdnParser, IsComposingParser, SdpMsrpDiagnosticsParser, Messaging Diagnostics nav page, SIP Ladder MESSAGE badges | feature/w090-msrp-lmpe-diagnostics | COMPLETE — read-only diagnostics only, no real MSRP session; 5 new test suites pass |
| W091 | Messaging Event Model + Safe Message Store — MessagingEvent (transport-independent), MessagingEventStore (bounded, mutex-protected), UI rewired to event store, max-events-retained config | feature/w091-messaging-event-store | COMPLETE — read-only, no new parsing, no real MSRP session; 1 new test suite (7 tests) pass |
| W092 | SIP MESSAGE Foundation — SipMessageComposer, CpimBuilder, SipAccount::sendMessage (pjsua2 Buddy::sendInstantMessage), SipManager::sendSipMessage, compose UI in Messaging Diagnostics page, enableSipMessage/enableCpim/requestImdnByDefault settings | feature/w092-sip-message-foundation | COMPLETE — basic send/receive only, MSRP still fully disabled, no delivery confirmation/IMDN generation; 1 new test suite (11 tests) pass |
| W093 | Incoming MESSAGE Handling + Message History — dedicated pjsua2 onInstantMessage/onInstantMessageStatus callbacks, MessageHistoryEntry/MessageHistoryStore (separate from MessagingEventStore, dedup logic), Message History UI with filters, outbound status confirmation (Submitted→Sent/Failed) | feature/w093-incoming-message-history | COMPLETE — no IMDN automation/Presence/XCAP/real MSRP; 1 new test suite (10 tests) pass |
| W094 | Windows Messaging/MSRP Diagnostics JSON Export — InteropTraceExporter (server-comparable schema for scripts/interop/compare-client-server-trace.py), Export Interop JSON button, sample export | feature/w094-windows-trace-json-export | COMPLETE — script not present in this repo (schema documented from spec instead); diagnostic-only, no MSRP activation; 1 new test suite (8 tests) pass |
| W095 | Deflate Decoding and RCS Payload Diagnostics Hardening — SipBodyExtractor (Content-Length byte-accurate body extraction), DeflateDecoder (zlib/raw-deflate/gzip, decompression-bomb-safe, no new dependency), RcsFtHttpParser + UrlRedactor, decode/RCS metadata through MessagingTraceEntry/MessagingEvent/InteropTraceExporter (schemaVersion 1→2), UI Encoding column + RCS/decode detail section | feature/w095-deflate-rcs-diagnostics | COMPLETE — diagnostic-only, no MSRP, no file download/auto-open, no network access; 4 new test suites (test_deflate_decoder, test_sip_body_extractor, test_rcs_ft_http_parser, test_url_redactor) plus coverage added to 3 existing suites; 46/46 tests pass |
| W096 | IMDN Foundation (RFC 5438) — ImdnGenerator (delivered/displayed/failed/error), auto-Delivered (default ON) + opt-in auto-Displayed/manual "Mark as Read" (default OFF), Message-ID correlation (MessageHistoryStore::correlateDelivery), ImdnParser extended (forbidden/processed), MessagingEvent/InteropTraceExporter diagnostics fields (generatedImdn/receivedImdn/correlatedMessageId/deliveryState, additive, schemaVersion stays 2) | feature/w096-imdn-foundation | COMPLETE — MSRP/Presence/XCAP untouched; coverage added to test_imdn_parser, test_sip_message_foundation, test_message_history; 46/46 tests pass |
| W097 | Active is-composing (RFC 3994) — IsComposingGenerator (active/idle/gone), TypingIndicatorController (debounced/throttled local typing state machine: active on first keystroke, periodic refresh keep-alive, idle after inactivity, gone after goneDelay or on send/close), Message History typing rows + live typing indicator (auto-clears after refresh), MessagingEvent/InteropTraceExporter diagnostics fields (generatedIsComposing/receivedIsComposing/typingState/typingRefresh/typingTimeout, additive, schemaVersion stays 2), Enable is-composing/Auto typing notifications settings (default ON) | feature/w097-is-composing | COMPLETE — MSRP/Presence/XCAP untouched; new test_typing_indicator_controller suite plus coverage added to test_is_composing_parser, test_sip_message_foundation, test_message_history; 47/47 tests pass |

---

## Module Status

| Module | Status | Notes |
|---|---|---|
| CMake build system | COMPLETE | Qt6, C++17, ENABLE_PJSIP, BUILD_TESTS |
| Dark GUI theme (QSS) | COMPLETE | Full dark theme |
| Main window layout | COMPLETE | Splitter-based, stable |
| Navigation rail | COMPLETE | Tab-based, wired |
| Account/contact sidebar | COMPLETE | Profile selector + account card + register buttons |
| Call panel + controls | COMPLETE | Dial row, mute, level meters, device selectors |
| Video panel | COMPLETE | SetParent/MoveWindow, overlays, mute, swap, camera selector |
| RTT panel | COMPLETE | Live typing, transcript, LMPE placeholder tab |
| LMPE panel | PLACEHOLDER | UI only — no LMPE logic |
| Diagnostics/log panel | COMPLETE | SIP Ladder tab + Log tab |
| Status bar | COMPLETE | Live SIP state + call state |
| Logger (core) | COMPLETE | Levels, categories, thread-safe signals |
| AppSettings | COMPLETE | QSettings wrapper |
| SipProfile model | COMPLETE | All fields, URI derivation |
| SipProfileManager | COMPLETE | CRUD, validation, persistence, credential helpers |
| CredentialStore | COMPLETE | Windows Credential Manager; MemoryBackend for tests |
| MediaDeviceManager | COMPLETE | Qt Multimedia backed; IMediaDeviceBackend |
| MediaDeviceSelectionModel | COMPLETE | Persistence + fallback to default |
| MediaPanel | COMPLETE | Mic/Speaker/Camera combos + Refresh |
| SipManager | COMPLETE | Full lifecycle; registration; call control; RTT session |
| RegistrationStateMachine | COMPLETE | 5-state SM, watchdog, transition guards |
| RegistrationRetryPolicy | COMPLETE | isRetryable(code), exponential backoff |
| RegistrationRefreshConfig | COMPLETE | 80%/30s margin, overrideDelayMs for tests |
| SipAccount | COMPLETE | pjsua2 account wrapper, queued callbacks |
| SipCall | COMPLETE | 9-state SM, stub + PJSIP paths, audio/video/RTT signals |
| CallStateMachine | COMPLETE | 9-state SM, watchdog, display text |
| AudioMediaManager | COMPLETE | attachCall/detachCall, mute, device selection, level forwarding |
| VideoMediaManager | COMPLETE | attachCall/detachCall, mute, camera selection |
| PjsipGdiRenderer | COMPLETE | Custom PJMEDIA GDI factory for HWND → device index mapping |
| CodecManager | COMPLETE | Audio/video codec matrix, priorities, negotiation logging |
| SipUriNormalizer | COMPLETE | bare users → sip:user@domain, full URI passthrough |
| SipMessageTrace / SipTraceLogger | COMPLETE | Credential redaction, export Text/JSON |
| SipLadderWidget | COMPLETE | paintEvent ladder, arrows, CSeq/Call-ID annotation, MESSAGE color + content-type badge |
| MessagingDiagnosticsStore | COMPLETE | Filters SipTraceLogger traces for MESSAGE/CPIM/IMDN/is-composing/RCS-FT-HTTP/MSRP-SDP, extracts body bytes + decodes Content-Encoding (Task W095), builds MessagingTraceEntry, export Text/JSON |
| SipBodyExtractor / DeflateDecoder | COMPLETE | Task W095 — Content-Length byte-accurate binary-safe body extraction; self-contained zlib/raw-deflate/gzip inflate (no new zlib dependency), decompression-bomb-safe limits; see [content-encoding-diagnostics.md](content-encoding-diagnostics.md) |
| RcsFtHttpParser / UrlRedactor | COMPLETE | Task W095 — read-only `application/vnd.gsma.rcs-ft-http+xml` descriptor parser (never fetches the URL/downloads/opens the file) + deterministic URL redaction (scheme/host/port + partial path, query/tokens always stripped); see [rcs-ft-http-diagnostics.md](rcs-ft-http-diagnostics.md) |
| CpimParser / ImdnParser / IsComposingParser | COMPLETE | RFC 3862 / RFC 5438 / RFC 3994 minimal parsers, pure Qt/text, unit tested. Task W096 extended `ImdnParser`/`ImdnInfo::Disposition` with `forbidden`/`processed`. Task W097 extended `IsComposingParser`/`IsComposingInfo` with `contentType` |
| ImdnGenerator | COMPLETE | Task W096 — pure Qt/text RFC 5438 report generator (delivered/displayed/failed/error), round-trips through the unmodified `ImdnParser`; see [imdn.md](imdn.md) |
| IsComposingGenerator / TypingIndicatorController | COMPLETE | Task W097 — pure Qt/text RFC 3994 notification generator (active/idle/gone) + a QTimer-driven local typing state machine with debounce/throttle; see [is-composing.md](is-composing.md) |
| SdpMsrpDiagnosticsParser | COMPLETE | Detects m=message, a=path/accept-types/setup/connection, session-id — detection only, no MSRP session |
| MessagingDiagnosticsPage | COMPLETE | "Messaging" nav page — diagnostics table (filters, Clear/Export Text/Export JSON/Export Interop JSON, sourced from MessagingEventStore, Task W091/W094) plus a Send SIP MESSAGE composer (Task W092) plus a Message History table with filters (Task W093) |
| MessagingEvent / MessagingEventStore | COMPLETE | Task W091 — transport-independent messaging model, bounded/mutex-protected store, maps MessagingTraceEntry (Task W090) without re-parsing; see [messaging-event-store.md](messaging-event-store.md) |
| SipMessageComposer / CpimBuilder | COMPLETE | Task W092 — pure Qt composer for outbound SIP MESSAGE (plain/HTML/CPIM, IMDN-request headers), no PJSIP/network dependency; see [sip-message.md](sip-message.md) |
| SipAccount::sendMessage / SipManager::sendSipMessage | COMPLETE | Task W092 — sends via a transient non-subscribing pjsua2 Buddy (Buddy::sendInstantMessage), fire-and-forget, non-blocking; MSRP untouched. Task W093 added onInstantMessageStatus confirmation (Submitted→Sent/Failed). Task W096 added `SipManager::sendImdnReport()`/`sendDisplayedImdnForEntry()` and Message-ID/Disposition-Notification header extraction in `SipAccount::onInstantMessage`. Task W097 added an is-composing branch in `onAccountInstantMessageReceived` (logs to Message History only) |
| MessageHistoryEntry / MessageHistoryStore | COMPLETE | Task W093 — dedicated onInstantMessage callback (From/To/Contact/Content-Type/body/Call-ID/profileId) + outbound composer feed a separate conversational history store with dedup and bounded preview; never touches MessagingEventStore; see [message-history.md](message-history.md). Task W096 added Message-ID correlation (`correlateDelivery`), `deliveryState`, IMDN-sent tracking (`markImdnSent`) — see [imdn.md](imdn.md) |
| InteropTraceExporter | COMPLETE | Task W094 — server-comparable JSON export (SIP MESSAGE/CPIM/IMDN/is-composing/MSRP-SDP), reads MessagingTraceEntry directly, no new parsing. Task W095 bumped schemaVersion 1→2, adding Content-Encoding decode metadata + rcsFileTransfer. Task W096 added generatedImdn/receivedImdn/correlatedMessageId/deliveryState (additive, schemaVersion stays 2). Task W097 added generatedIsComposing/receivedIsComposing/typingState/typingRefresh/typingTimeout (additive, schemaVersion stays 2); see [windows-trace-json-export.md](windows-trace-json-export.md) |
| RttSession | COMPLETE | 5-state SM, sendText, onCallMediaStateChanged |
| RttPanel | COMPLETE | TX delta, RX accumulation, BS, CR→transcript |
| FindPJSIP.cmake | COMPLETE | PJSIP discovery, PJSIP::pjsua2 imported target |
| SIP registration (live) | COMPLETE | REGISTER/UNREGISTER, 401, refresh, retry — live validated |
| SIP call control (live) | COMPLETE | INVITE, BYE, Hold, outbound proxy — live validated |
| Audio calls (live) | COMPLETE | G.722 + G.711, level meters, device selection — live validated |
| Video calls (live) | COMPLETE | VP8, DirectShow, SetParent render — live validated |
| RTT RFC 4103 (live) | COMPLETE | TX/RX char-by-char, RED lv2, cleanup — live validated |
| EmergencyCallProfile | SKELETON | Validation + factory. No SIP, no PJSIP. |
| EmergencyCallStateMachine | SKELETON | 8-state SM. Independent of PJSIP. |
| EmergencyLocationProvider | SKELETON | Abstract interface + NullLocationProvider (NotImplemented) |
| EmergencyCallController | COMPLETE | Orchestration. Wired to SipManager::makeEmergencyCall() via CallPanel (Task 42). |
| ManualLocationProvider (UI) | COMPLETE | Manual lat/lon/unc input in CallPanel emergency row → StaticLocationProvider.setLocation() → PIDF-LO preview (Task 43). |
| SipCall::sendLocationUpdate | COMPLETE | SIP UPDATE with multipart PIDF-LO body. PJSIP mode: pjCall->update(). Stub mode: returns false + logs limitation (Task 43). |
| SipManager::sendEmergencyLocationUpdate | COMPLETE | Delegates to activeCall->sendLocationUpdate(). Guards: emergencyCall flag, active call required (Task 43). |
| EmergencyInviteBuilder | COMPLETE | Declarative INVITE builder. No PJSIP. 15 tests pass. |
| EmergencyInvite | COMPLETE | Pure data struct. requestUri, headers, mediaPolicy, location flags, contentId. |
| EmergencyLocation | COMPLETE | Geodetic location struct. WGS-84. Validation. |
| PidfLoBuilder | COMPLETE | RFC 4119 PIDF-LO XML builder. Point/Circle. No PJSIP. |
| StaticLocationProvider | COMPLETE | Location provider backed by static EmergencyLocation. |
| SipCallOptions | COMPLETE | Generic per-call SIP options. Header injection, media policy. |
| SipCall::makeCallWithOptions | COMPLETE | INVITE + custom headers via pjsua2 txOption. Normal makeCall() unchanged. |
| SipManager::makeEmergencyCall | COMPLETE | Emergency call entry point. Uses SipCallOptions. |
| EmergencyCallAdapter | COMPLETE | Bridges EmergencyInvite → SipCallOptions. Generates Content-ID. |
| EmergencyMultipartBuilder | COMPLETE | Declarative MIME part builder. PIDF-LO part with Content-ID header. No PJSIP. |
| test_emergency_protocol_validation | COMPLETE | 12 subtests — negative/positive protocol validation. No PJSIP. |
| live_emergency_call_probe | COMPLETE | CLI live probe. Full chain to PJSIP. Live validated (Task 41) — INVITE sent, 404 from Kamailio (PSAP absent, expected). |
| LMPE messaging | NOT STARTED | Messaging Diagnostics foundation (Task W090/W091), basic SIP MESSAGE send/receive (Task W092), Message History (Task W093), interop JSON export (Task W094), Content-Encoding/RCS diagnostics hardening (Task W095), IMDN Foundation (Task W096), and Active is-composing (Task W097) exist but LMPE encode/decode is not implemented |
| SIP MESSAGE / CPIM / IMDN / is-composing diagnostics | COMPLETE | Task W090 — read-only, see [messaging-diagnostics.md](messaging-diagnostics.md) |
| Transport-independent messaging event model | COMPLETE | Task W091 — see [messaging-event-store.md](messaging-event-store.md) |
| SIP MESSAGE Foundation (send/receive) | COMPLETE | Task W092 — plain/HTML/CPIM body, optional IMDN-request headers; no IMDN generation, MSRP untouched; see [sip-message.md](sip-message.md) |
| Incoming MESSAGE callback + Message History | COMPLETE | Task W093 — dedicated onInstantMessage/onInstantMessageStatus callbacks, conversational history with filters, outbound Sent/Failed confirmation, dedup; no IMDN automation/Presence/XCAP/MSRP; see [message-history.md](message-history.md) |
| Windows Trace JSON Export (interop) | COMPLETE | Task W094 — server-comparable JSON schema for scripts/interop/compare-client-server-trace.py (not present in this repo — schema documented from spec + sample export shipped); diagnostic-only; see [windows-trace-json-export.md](windows-trace-json-export.md) |
| Content-Encoding deflate decoding + RCS FT HTTP diagnostics | COMPLETE | Task W095 — binary-safe body extraction, zlib/raw-deflate/gzip decoder with decompression-bomb limits, read-only RCS file-transfer descriptor parser with URL redaction; diagnostic-only, no file download/network access; see [content-encoding-diagnostics.md](content-encoding-diagnostics.md) and [rcs-ft-http-diagnostics.md](rcs-ft-http-diagnostics.md) |
| IMDN Foundation (generation, correlation, auto-Delivered/Displayed) | COMPLETE | Task W096 — RFC 5438 report generation, Message-ID correlation, auto-Delivered (default ON)/opt-in auto-Displayed (default OFF)/manual "Mark as Read"; MSRP/Presence/XCAP untouched; see [imdn.md](imdn.md) |
| Active is-composing (generation, typing state machine, indicator) | COMPLETE | Task W097 — RFC 3994 notification generation, debounced/throttled local typing state machine, Message History typing rows + auto-expiring live indicator, Enable is-composing/Auto typing notifications (default ON); MSRP/Presence/XCAP untouched; see [is-composing.md](is-composing.md) |
| SIP Presence Foundation (SUBSCRIBE/NOTIFY/PIDF) | COMPLETE (foundation) | Task W098 — PIDF parser + presence model, live pjsua2-Buddy subscribe/unsubscribe/refresh lifecycle with auto-resubscribe backoff, dedicated `PresenceStore`, new "Presence" nav page, SIP Ladder badges, `presenceEvents` in Interop JSON export; own-status Publish is experimental (needs re-registration, depends on server PUBLISH support); everything disabled by default; XCAP/resource lists explicitly out of scope (Task W099); MSRP untouched; see [presence.md](presence.md) |
| XCAP Foundation (client + diagnostics) | COMPLETE (foundation) | Task W099 — asynchronous RFC 4825 GET/PUT/DELETE/HEAD via `QNetworkAccessManager`, generic-XML validation before PUT, Basic auth (proactive header) + Digest auth (via Qt's built-in challenge/response), passwords via `CredentialStore` (never stored/logged in clear), URL-redacted "XCAP" nav page + operation log, `xcapEvents` in Interop JSON export; every AUID (pres-rules/resource-lists/rls-services/xcap-caps) treated as generic XML — no Resource Lists/policy semantics yet; disabled by default; plain HTTP, never drawn as SIP traffic in the Ladder; MSRP untouched; see [xcap.md](xcap.md) |
| MSRP Foundation (protocol stack + transport) | COMPLETE (foundation) | Task W100 — full RFC 4975/4976 protocol stack built in Qt (`QTcpSocket`/`QSslSocket`), independent of pjsua2 (no MSRP support in this pjproject build): SDP negotiation model, path parser/builder, session state machine, incremental binary-safe frame parser/serializer, TCP+TLS transport, SEND/response/REPORT with transaction tracking, chunking + Byte-Range reassembly, payload dispatch reusing CpimParser/ImdnParser/IsComposingParser/MessageHistoryStore unchanged, `MessagingTransportPolicy` (SIP-MESSAGE-only/MSRP-preferred/MSRP-required/automatic with fallback), new "MSRP" nav page, `msrpSessions`/`msrpEvents` in Interop JSON export; disabled by default; local client/server test harness over loopback TCP passes; no live SIP-Server-RTT interop test executed (no server available); see [msrp-foundation.md](msrp-foundation.md) |
| Live MSRP SIP Integration | COMPLETE (offerer path) | Task W101 — corrected W100's "onCallSdpCreated is read-only" conclusion: `m=message` is now spliced directly into the live `pjmedia_sdp_session` (public `pjmedia/sdp.h` API only, no vendored pjproject edit), proven by a wire-bytes test (`test_msrp_sip_media_injector`); each call starts its own MSRP listener before offering it; SIP↔MSRP dialog mapping (`sipHeaderCallId`, `mediaIndex`) added to `MsrpSessionInfo`; `MessagingTransportPolicy` now actually gates `SipManager::sendSipMessage()` (no dual-send, automatic recovery once MSRP re-establishes, no persistent fallback state); SEND→response/REPORT now feeds `MessageHistoryStore` (inbound) and is logged (delivery status, not yet correlated — distinct Message-ID space); answering a peer-initiated `m=message` offer, the UI diagnostics page, and export schemaVersion 3 are NOT implemented in this pass; 67/67 CTest PASS, zero regressions; no live SIP-Server-RTT/Blink/AG Projects/Linphone capture executed (no server/reference client available in this environment) — see [msrp-live-sdp-integration.md](msrp-live-sdp-integration.md), [msrp-peer-association.md](msrp-peer-association.md), [msrp-fallback.md](msrp-fallback.md), [agent-results/W101-live-msrp-sip-integration-result.md](agent-results/W101-live-msrp-sip-integration-result.md) |
| MSRP Live Interoperability | COMPLETE (structural; live interop NOT RUN) | Task W102 — peer-initiated `m=message` offers are now answered at the correct SDP index (`MsrpSipMediaInjector::answerMessageMediaAtIndex`) instead of always left rejected; setup role is always the structural complement of the remote's proposal (rules out active/active and passive/passive by construction); `MsrpSessionInfo::role`/`remoteSetup`/`negotiationState`/`peerAssociation` now real and exported (schemaVersion → 3); MSRP page shows Negotiation/Peer Association columns; new deterministic `TraceComparator`; TLS and early-dialog/forking behavior audited (no code change needed, gaps documented); incoming-connection validation beyond IP/port and multi-session-per-SDP support remain NOT implemented (documented, tracked for W103); 68/68 CTest PASS, zero regressions; no live SIP-Server-RTT/Blink/AG Projects/Linphone/TLS/forking test executed (no environment available) — see [msrp-offer-answer.md](msrp-offer-answer.md), [msrp-live-interoperability.md](msrp-live-interoperability.md), [agent-results/W102-msrp-live-interoperability-result.md](agent-results/W102-msrp-live-interoperability-result.md) |
| LMPE Foundation | BLOCKED (codec) | Task W103 — per its own starting rule, the real ETSI TS 103 698 wire format must be confirmed against the SIP-Server-RTT repository or another authoritative source before any codec code is written; `SIP-Server-RTT` does not exist as a git remote or sibling directory anywhere in this workspace and `github.com/Iryme/SIP-Server-RTT` returns HTTP 404; no other authoritative LMPE wire-format source (content-type/framing/version/required fields) was found either — `docs/lmpe.md` and `docs/etsi-compatibility/ts-103-698-mapping.md` in this repo, and the `lmpe/` module in the sibling `PJSIP-windows-app-emergency` repo, are all placeholders with no real format defined; per the rule, no protocol was invented and no codec was implemented; only a neutral `LmpeCodec` interface and a fail-closed `UnconfirmedLmpeCodec` stub (always reports `isFormatConfirmed()==false`, never claims success) were added as a seam for a future task, guarded by `test_lmpe_codec_unconfirmed`; existing LMPE UI tab (local-echo placeholder) untouched; 69/69 CTest PASS, zero regressions — see [agent-results/W103-lmpe-foundation-result.md](agent-results/W103-lmpe-foundation-result.md) |
| MSRP File Transfer (RFC 5547) | COMPLETE (structural; live interop NOT RUN) | Task W104 — file transfer reuses the existing MSRP session/chunker/assembler pipeline unchanged: new `MsrpFileSelector` (RFC 5547 `a=file-selector` parse/build + peer-filename sanitization), `Content-Disposition` frame header threaded through `MsrpFrame`/parser/serializer/chunker/assembler alongside the existing `Content-Type`, `MsrpSession::sendFile()` (bounded in-memory read + SHA-1 + sanitized `attachment; filename=...`), new `fileTransferReceived` signal (fires additively, never instead of the existing `payloadReceived`), new `MsrpFileReceiver` (atomic `QSaveFile` write + SHA-1/SHA-256 verification, path always caller-chosen, never peer-derived), "Send File…" button + save-on-receive dialog on the MSRP page's manual test session; no live SIP-Server-RTT/third-party MSRP client interop test executed (no environment available, same limitation as W100–W103); 71/71 CTest PASS, zero regressions — see [msrp-file-transfer.md](msrp-file-transfer.md), [agent-results/W104-msrp-file-transfer-result.md](agent-results/W104-msrp-file-transfer-result.md) |
| MSRP Advanced Hardening (inbound connection authentication) | COMPLETE | Task W105 — found and fixed a genuine gap: `MsrpTcpTransport::listenPassive()` accepts the first inbound TCP connection unconditionally (matches RFC 4975's passive-role model), but `MsrpSession::handleFrame()` previously processed any inbound SEND/REPORT from that connection without checking who it came from, so a local process winning the TCP accept race before the real SDP-negotiated peer would be silently trusted; `MsrpSession::toPathTargetsThisSession()` now enforces RFC 4975 §7.1 (reject with `403 Forbidden` + logged `MsrpDiagnosticsEvent` when the request's To-Path session-id doesn't match this session's own negotiated local session-id), verified by a new regression test proving a rogue connection's SEND never reaches `payloadReceived` while all legitimate-peer traffic is unaffected; no schema change; 71/71 CTest PASS, zero regressions — see [msrp-security.md](msrp-security.md), [agent-results/W105-msrp-advanced-hardening-result.md](agent-results/W105-msrp-advanced-hardening-result.md) |
| MSRP diagnostics (SDP detection) | COMPLETE | Task W090 — detection only, no real MSRP session, see [msrp-diagnostics.md](msrp-diagnostics.md) |
| MSRP session (real transport) | NOT STARTED | Explicitly out of scope for Task W090 |
| ETSI TS 103 479 | NOT STARTED | Blocked on Task 37–38 |
| ETSI TS 103 480 | NOT STARTED | Blocked on Task 37–38 |
| ETSI TS 103 698 | NOT STARTED | Blocked on Task 37 (location provider) |
| Incoming call notification | NOT STARTED | |
| Call history | NOT STARTED | |
| Network recovery | NOT STARTED | |
| Linux/macOS credential backends | NOT STARTED | |
| Debug bundle export | NOT STARTED | |

---

## Known Limitations (current, post-Task-34)

- No automatic registration on startup or profile selection.
- Network-change recovery not implemented.
- One active call at a time; conference not supported.
- VP9 / H.264 not available (VP8 only).
- Audio/video hot-swap during a call not implemented.
- Hot-plug device detection not forwarded to MediaDeviceManager.
- Incoming call not tested live (GUI flow only verified in stub mode).
- ETSI / NG112 not implemented.
- CredentialStore: Windows backend only; Linux/macOS return failure.
- Contact list hardcoded; contact management not implemented.
- Debug bundle export not implemented.
- SIP trace: raw SIP capture not implemented (synthetic traces only).

---

## Branch Lineage Note

No `main` branch exists. `origin/HEAD` → `feature/project-skeleton`.
Each task branch is a linear child of the previous one. Tasks 2–5 remain on a
separate unmerged lineage (richer Logger/DiagnosticsPanel/AppSettings variants);
the active code uses the simpler Task-1 implementations of these.
