# Project Status

Last updated: 2026-07-09
Current task count: 45 of N (Tasks 1–28.6 + Task 34–43 + Task W090–W091 complete)
Active branch: `feature/w091-messaging-event-store`

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
| MessagingDiagnosticsStore | COMPLETE | Filters SipTraceLogger traces for MESSAGE/CPIM/IMDN/is-composing/MSRP-SDP, builds MessagingTraceEntry, export Text/JSON |
| CpimParser / ImdnParser / IsComposingParser | COMPLETE | RFC 3862 / RFC 5438 / RFC 3994 minimal parsers, pure Qt/text, unit tested |
| SdpMsrpDiagnosticsParser | COMPLETE | Detects m=message, a=path/accept-types/setup/connection, session-id — detection only, no MSRP session |
| MessagingDiagnosticsPage | COMPLETE | Read-only "Messaging" nav page — table, filters, Clear/Export Text/Export JSON; sourced from MessagingEventStore (Task W091) |
| MessagingEvent / MessagingEventStore | COMPLETE | Task W091 — transport-independent messaging model, bounded/mutex-protected store, maps MessagingTraceEntry (Task W090) without re-parsing; see [messaging-event-store.md](messaging-event-store.md) |
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
| LMPE messaging | NOT STARTED | Messaging Diagnostics foundation (Task W090/W091) observes SIP MESSAGE traffic but does not implement LMPE encode/decode |
| SIP MESSAGE / CPIM / IMDN / is-composing diagnostics | COMPLETE | Task W090 — read-only, see [messaging-diagnostics.md](messaging-diagnostics.md) |
| Transport-independent messaging event model | COMPLETE | Task W091 — see [messaging-event-store.md](messaging-event-store.md) |
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
