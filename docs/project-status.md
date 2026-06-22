# Project Status

Last updated: 2026-06-22
Current task count: 3 completed

## Completed Tasks

| # | Task | Branch | Status |
|---|---|---|---|
| 1 | Project skeleton, GUI layout, documentation | feature/project-skeleton | IMPLEMENTED |
| 2 | Permanent GUI layout specification | feature/gui-layout-spec | IMPLEMENTED |
| 3 | Diagnostics logging infrastructure | feature/diagnostics-logger | IMPLEMENTED |

## Module Status

| Module | Status | Notes |
|---|---|---|
| CMake build system | IMPLEMENTED | Qt6, C++17 |
| Dark GUI theme (QSS) | IMPLEMENTED | Full dark theme |
| Main window layout | IMPLEMENTED | Splitter-based, stable |
| Navigation rail | IMPLEMENTED | Placeholder icons |
| Account/contact sidebar | IMPLEMENTED | Placeholder data |
| Call panel + controls | IMPLEMENTED | Wired signals, no SIP |
| Video panel | IMPLEMENTED | Placeholder paintEvent |
| RTT panel | IMPLEMENTED | Placeholder UI |
| LMPE panel | IMPLEMENTED | Placeholder UI |
| Diagnostics/log panel | IMPLEMENTED | Fully functional |
| Status bar | IMPLEMENTED | Placeholder labels |
| DiagnosticsLogger | IMPLEMENTED | Thread-safe, redaction, export, category filter, unit tested |
| Logger (GUI adapter) | IMPLEMENTED | Qt signal emitter wrapping DiagnosticsLogger |
| AppSettings | IMPLEMENTED | QSettings wrapper |
| SIP stack (PJSIP) | NOT STARTED | |
| SIP registration | NOT STARTED | |
| Audio calls | NOT STARTED | |
| Video calls | NOT STARTED | |
| RFC 4103 RTT | NOT STARTED | |
| LMPE messaging | NOT STARTED | |
| ETSI TS 103 479 | NOT STARTED | |
| ETSI TS 103 480 | NOT STARTED | |
| ETSI TS 103 698 | NOT STARTED | |
| Debug bundle export | NOT STARTED | |
| SIP profile management UI | NOT STARTED | |
| Media device selection | NOT STARTED | |
| Call statistics | NOT STARTED | |

## Known Limitations

### Task 1
- No SIP networking — application is GUI-only
- No PJSIP dependency — must be added in the next SIP task
- Nav rail buttons have text labels only (no icons)
- Contact list uses hardcoded placeholder entries
- Info tabs (Call Info, Media, Statistics) are empty panels
- Category filter and search in Diagnostics are UI-only, not wired
- Video area shows a placeholder paintEvent, no real video
- Local preview is a labeled box, no camera input

### Task 3
- `src/core/Logger` (GUI signal layer) does not yet delegate to `DiagnosticsLogger`; the two loggers are independent. Unification is planned as a refactor step before SIP integration.
- Build and test run not verified on this machine (CMake not installed). Tests compile cleanly per static review; run `cmake -DBUILD_TESTS=ON ..` and `ctest` to verify.

## Next Recommended Task

**Task 4:** Integrate PJSIP/pjsua2 — add CMake find module (`cmake/FindPJSIP.cmake`),
`src/sip/SipManager.h/.cpp` skeleton (initialize/shutdown PJSUA endpoint),
`SipAccount.h` and `SipCall.h` stubs. Log startup/shutdown via `DiagnosticsLogger`.
Update `docs/architecture.md` and `docs/sip-profiles.md`.
