# Project Status

Last updated: 2026-06-23
Current task count: 18 of N

## Completed Tasks

| # | Task | Branch | Status |
|---|---|---|---|
| 1 | Project skeleton, GUI layout, documentation | feature/project-skeleton | IMPLEMENTED |
| 2 | GUI layout specification | feature/gui-layout-spec | IMPLEMENTED ON SEPARATE BRANCH |
| 3 | Diagnostics logger | feature/diagnostics-logger | IMPLEMENTED ON SEPARATE BRANCH |
| 4 | Diagnostics GUI console | feature/diagnostics-gui-console | IMPLEMENTED ON SEPARATE BRANCH |
| 5 | Persistent application settings | feature/persistent-settings | IMPLEMENTED ON SEPARATE BRANCH |
| 6 | SIP profile model, persistence, GUI placeholders | feature/sip-profile-manager | IMPLEMENTED |
| 7 | Secure credential storage (CredentialStore, Windows Credential Manager) | feature/secure-credential-storage | IMPLEMENTED |
| 8 | SIP Profile Editor Dialog — Add/Edit/Delete with CredentialStore integration | feature/sip-profile-editor | IMPLEMENTED |
| 9 | Media device enumeration — MediaDeviceManager, MediaPanel, persistence, fallback | feature/media-device-enumeration | IMPLEMENTED |
| 10 | PJSIP build integration — FindPJSIP.cmake, ENABLE_PJSIP option, SipManager lifecycle skeleton | feature/pjsip-build-integration | IMPLEMENTED |
| 11 | Project Handoff 001 — formal project snapshot and continuation prompt | feature/project-handoff-001 | IMPLEMENTED |
| 12 | SIP Register / Unregister — active profile, secure credential lookup, pjsua2 callbacks, GUI status | feature/sip-registration | IMPLEMENTED |
| 13 | Registration State Machine — explicit 5-state SM, transition guards, watchdog timeout, button disable | feature/registration-state-machine | IMPLEMENTED |
| 14 | Registration Retry/Backoff — RegistrationRetryPolicy, exponential backoff, retryScheduled signal | feature/registration-retry-backoff | IMPLEMENTED |
| 15 | Registration Expiry & Auto Re-REGISTER — RegistrationRefreshConfig, refresh timer, refresh failure → retry | feature/registration-expiry-refresh | IMPLEMENTED |
| 16 | Profile Switch Sequencing — switchActiveProfile(), pending-switch guard, UNREGISTER old before registering new | feature/profile-switch-sequencing | IMPLEMENTED |
| 17 | SIP Call State Machine — CallStateMachine (9 states), SipCall API, SipManager call control, CallPanel wiring | feature/call-state-machine | IMPLEMENTED |
| 18 | Audio Media Integration — AudioMediaManager, mute/unmute, level meters, device selection, PJSIP bridge wiring | feature/audio-media | IMPLEMENTED |

> **Branch lineage warning:** the repository has no `main` branch, and Tasks 2-5
> are not ancestors of the active Tasks 6-10 lineage. The active code uses the
> simpler `Logger`, `DiagnosticsPanel`, and `AppSettings` implementations. See
> `docs/project-handoff/handoff-001.md` before reconciling these branches.

## Module Status

| Module | Status | Notes |
|---|---|---|
| CMake build system | IMPLEMENTED | Qt6, C++17 |
| Dark GUI theme (QSS) | IMPLEMENTED | Full dark theme |
| Main window layout | IMPLEMENTED | Splitter-based, stable |
| Navigation rail | IMPLEMENTED | Placeholder icons |
| Account/contact sidebar | IMPLEMENTED | Profile selector + account card |
| Call panel + controls | IMPLEMENTED | Wired signals, no SIP |
| Video panel | IMPLEMENTED | Placeholder paintEvent |
| RTT panel | IMPLEMENTED | Placeholder UI |
| LMPE panel | IMPLEMENTED | Placeholder UI |
| Diagnostics/log panel | IMPLEMENTED | Fully functional |
| Status bar | IMPLEMENTED | Live SIP registration state; call/media statistics remain placeholders |
| Logger (core) | IMPLEMENTED | Levels, categories, signals |
| AppSettings | IMPLEMENTED | QSettings wrapper |
| SipProfile model | IMPLEMENTED | All fields, URI derivation |
| SipProfileManager | IMPLEMENTED | CRUD, validation, persistence, active selection, credential helpers |
| CredentialStore | IMPLEMENTED | Windows Credential Manager backend; MemoryCredentialBackend for tests |
| MediaDevice model | IMPLEMENTED | id, displayName, type, isDefault, isAvailable |
| MediaDeviceManager | IMPLEMENTED | Qt Multimedia backed; pluggable IMediaDeviceBackend |
| MediaDeviceSelectionModel | IMPLEMENTED | Persistence + fallback to default |
| MediaPanel | IMPLEMENTED | Microphone/Speaker/Camera combos + Refresh button in Media tab |
| SipManager | IMPLEMENTED | Lifecycle + register/unregister + switchActiveProfile() sequencing; stub + PJSIP branches |
| RegistrationStateMachine | IMPLEMENTED | 5-state explicit SM; transition table; watchdog timeout; diagnostics; Registered→RegistrationFailed for refresh failure |
| RegistrationRetryPolicy | IMPLEMENTED | isRetryable(code), delayForAttempt(n) exponential backoff |
| RegistrationRefreshConfig | IMPLEMENTED | delayMsForExpiry(expiry): 80% ratio / 30s margin; overrideDelayMs for tests |
| SipAccount | IMPLEMENTED | pjsua2 account creation, REGISTER/UNREGISTER, queued callbacks; refreshRegistration(); registrationExpiryReceived signal |
| SipCall | IMPLEMENTED | makeCall/answer/reject/hangup/hold/resume; owns CallStateMachine; setMuted/isMuted; audioMediaConnected/Disconnected signals; level timer (PJSIP); stub + PJSIP paths |
| CallStateMachine | IMPLEMENTED | 9-state explicit SM; transition table; watchdog timeout (OutgoingInit/Disconnecting); diagnostics |
| AudioMediaManager | IMPLEMENTED | attachCall/detachCall lifecycle; setMuted; setMicrophone/Speaker (persisted); inputLevel/outputLevel forwarding; Qt-only (no pjsua2.hpp) |
| cmake/FindPJSIP.cmake | IMPLEMENTED | Searches PJSIP_DIR, pkg-config, system paths |
| SIP registration | IMPLEMENTED | Manual register/unregister; exponential retry on transient failures; auto-refresh before expiry; deterministic profile-switch sequencing |
| SIP call control | IMPLEMENTED | makeCall/answer/reject/hangup/hold/resume; 9-state SM; stub (test-only) + PJSIP scaffolding |
| Audio calls | SCAFFOLDED | AudioMediaManager + PJSIP bridge wiring complete; untested without live PJSIP installation |
| Video calls | NOT STARTED | |
| RFC 4103 RTT | NOT STARTED | |
| LMPE messaging | NOT STARTED | |
| ETSI TS 103 479 | NOT STARTED | |
| ETSI TS 103 480 | NOT STARTED | |
| ETSI TS 103 698 | NOT STARTED | |
| Debug bundle export | NOT STARTED | |
| Profile editor dialog | IMPLEMENTED | SipProfileEditorDialog — Add/Edit/Delete with password via CredentialStore |
| Media device selection | NOT STARTED | |
| Call statistics | NOT STARTED | |
| Credential / keychain storage | IMPLEMENTED (Windows) | Linux/macOS deferred (ADR-011) |

## Known Limitations (Task 1)

- No SIP networking — application is GUI-only
- No PJSIP dependency — must be added in the next SIP task
- Nav rail buttons have text labels only (no icons)
- Contact list uses hardcoded placeholder entries
- Info tabs (Call Info, Media, Statistics) are empty panels
- Category filter and search in Diagnostics are UI-only, not wired
- Video area shows a placeholder paintEvent, no real video
- Local preview is a labeled box, no camera input

## Known Limitations (Task 6)

- No profile editor dialog — Add/Edit buttons in SidebarPanel log "not implemented"
- No SIP registration — SipProfileManager is persistence-only; PJSIP integration is deferred
- `emergencyServiceUri` is persisted but not used — ETSI emergency integration is future work
- Contact list remains hardcoded; contacts are a separate future task

## Known Limitations (Task 7)

- `CredentialStore` is implemented for Windows only; Linux/macOS backends not yet written
- `CRED_PERSIST_LOCAL_MACHINE` — credentials are bound to the current machine, no roaming
- No credential migration or export path (by design — credentials are not application data)

## Known Limitations (Task 8)

- Profile editor dialog is a plain modal — no in-dialog confirmation for destructive changes
- "Custom SIP Headers" in the Advanced section is a UI placeholder, not yet wired
- No inline error highlighting — validation errors are shown in a `QMessageBox`, not inline
- Contact list in `SidebarPanel` remains hardcoded placeholder entries (separate future task)

## Known Limitations (Task 10)

- `ENABLE_PJSIP` defaults to `OFF`; no real SIP calls or registration until enabled + PJSIP installed
- `SipAccount` and `SipCall` are header-only stubs — no account creation or call management yet
- PJSIP thread safety: when PJSIP is compiled in, PJSIP callbacks must be dispatched to the Qt main thread via `QMetaObject::invokeMethod` (not yet implemented — deferred to registration task)
- The status bar label `SIP: Stub SIP backend (ready)` is informational only; it does not reflect real registration state

## Known Limitations (Task 9)

- Audio level meters are static/inactive — no real microphone capture yet
- Hot-plug detection: `QMediaDevices::audioInputsChanged` / `videoInputsChanged` signals are not yet forwarded into `MediaDeviceManager`; user must press Refresh
- Camera enumeration requires Qt Multimedia camera permissions on macOS
- Speakers may not enumerate on headless/virtual-machine systems
- Video preview in `VideoPanel` remains a placeholder paintEvent — no real camera stream

## Known Limitations (Task 12)

- Real PJSIP registration was not integration-tested on this machine because PJSIP is not installed; stub mode and guarded APIs were verified.
- Registration is manual; there is no startup/profile-change auto-register behavior.
- Profile switching does not wait for an old account's unregister response before account replacement.

## Known Limitations (Task 13)

- No automatic registration on startup or profile selection.
- ~~No refresh scheduling before registration expiry.~~ (Task 15 done)
- ~~No retry/backoff on transient failures.~~ (Task 14 done)
- Network-change recovery not implemented.
- One account supported at a time; multi-account deferred.

## Known Limitations (Task 14)

- Retry counter resets on exhaustion; no persistent backoff across app restarts.

## Known Limitations (Task 15)

- Expiry is only extracted from PJSIP AccountInfo; if the server omits it, the configured default (300 s) is used.
- No automatic registration on startup or profile selection.
- ~~Profile switch does not wait for old account UNREGISTER response before replacing.~~ (Task 16 done)
- Network-change recovery not implemented.
- CredentialStore Linux/macOS backends not yet implemented.

## Known Limitations (Task 16)

- No automatic registration on startup or when a profile is selected for the first time.
- Network-change recovery not implemented.
- Watchdog timeout during profile switch proceeds with the new profile registration even if the old UNREGISTER was not acknowledged by the server; the old registration may live until server-side expiry.
- One account is supported at a time; multi-account is deferred.

## Known Limitations (Task 17)

- No automatic registration on startup or profile selection.
- Network-change recovery not implemented.
- Call audio/video/RTT media not yet wired — SipCall controls call state only; media setup is deferred.
- SipCall PJSIP path requires pj::Account access from SipAccount; full PJSIP call integration is scaffolded but requires a live PJSIP installation to test.
- One concurrent call supported; multi-party/conference is deferred.
- Call duration timer is a UI placeholder — no elapsed-time counter yet.
- Incoming call identification (name resolution) not yet implemented — remote URI only.

## Known Limitations (Task 18)

- Audio bridge tested through compilation only; live end-to-end audio requires `ENABLE_PJSIP=ON` with a real PJSIP installation.
- Device hot-swap during a call: selection is persisted and takes effect on the next call. PJSIP integer device-index mapping from Qt Multimedia string IDs is not yet implemented.
- Audio level meters always show 0 in stub mode (no PJSIP).
- Video and RTT media are still NOT STARTED.
- `tests/CMakeLists.txt`: any test target that compiles `SipManager.cpp` must include `AUDIO_MEDIA_SOURCES` (AudioMediaManager.cpp + MediaDeviceManager.cpp + MediaDeviceSelectionModel.cpp + QtMediaDeviceBackend.cpp) and link `Qt6::Multimedia`.

## Next Recommended Task

**Task 19:** Startup auto-register — register the active profile automatically on `initialize()` when a profile with a credential is configured.
