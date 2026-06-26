# Project Status

Last updated: 2026-06-26
Current task count: 28 of N (Tasks 1–28.6 complete)
Active branch: `feature/project-handoff-002` HEAD `96ee7b4`

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
| ETSI / NG112 | 34+ | NOT STARTED |

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
| SipLadderWidget | COMPLETE | paintEvent ladder, arrows, CSeq/Call-ID annotation |
| RttSession | COMPLETE | 5-state SM, sendText, onCallMediaStateChanged |
| RttPanel | COMPLETE | TX delta, RX accumulation, BS, CR→transcript |
| FindPJSIP.cmake | COMPLETE | PJSIP discovery, PJSIP::pjsua2 imported target |
| SIP registration (live) | COMPLETE | REGISTER/UNREGISTER, 401, refresh, retry — live validated |
| SIP call control (live) | COMPLETE | INVITE, BYE, Hold, outbound proxy — live validated |
| Audio calls (live) | COMPLETE | G.722 + G.711, level meters, device selection — live validated |
| Video calls (live) | COMPLETE | VP8, DirectShow, SetParent render — live validated |
| RTT RFC 4103 (live) | COMPLETE | TX/RX char-by-char, RED lv2, cleanup — live validated |
| LMPE messaging | NOT STARTED | |
| ETSI TS 103 479 | NOT STARTED | |
| ETSI TS 103 480 | NOT STARTED | |
| ETSI TS 103 698 | NOT STARTED | |
| Incoming call notification | NOT STARTED | |
| Call history | NOT STARTED | |
| Network recovery | NOT STARTED | |
| Linux/macOS credential backends | NOT STARTED | |
| Debug bundle export | NOT STARTED | |

---

## Known Limitations (current, post-Task-28.6)

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
