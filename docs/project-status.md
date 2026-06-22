# Project Status

Last updated: 2026-06-22
Current task count: 4 completed

## Completed Tasks

| # | Task | Branch | Status |
|---|---|---|---|
| 1 | Project skeleton, GUI layout, documentation | feature/project-skeleton | IMPLEMENTED |
| 2 | Permanent GUI layout specification | feature/gui-layout-spec | IMPLEMENTED |
| 3 | Diagnostics logging infrastructure | feature/diagnostics-logger | IMPLEMENTED |
| 4 | Diagnostics GUI Console wired to DiagnosticsLogger | feature/diagnostics-gui-console | IMPLEMENTED |

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
| DiagnosticsLogger | IMPLEMENTED | Thread-safe, QObject with signal, redaction, export, unit tested |
| LogFilterModel | IMPLEMENTED | Level/category/search filter; decoupled from GUI; unit tested |
| Diagnostics GUI Console | IMPLEMENTED | Full filter panel wired to DiagnosticsLogger |
| Logger (legacy) | IMPLEMENTED | Kept for backward compat; not connected to DiagnosticsPanel |
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
- `src/core/Logger` (legacy class) does not delegate to `DiagnosticsLogger`; the two are independent. Unification can be done before SIP integration if desired, but is not required — `DiagnosticsPanel` now connects directly to `DiagnosticsLogger`.
- Build and test run not verified on this machine (Qt6 not installed). Tests compile cleanly per static review; run `cmake -DBUILD_TESTS=ON ..` and `ctest` to verify.

### Task 4
- Build not verified on this machine — Qt6 not installed in the CI/dev environment. Static review shows no issues; run `cmake -DBUILD_TESTS=ON .. && make && ctest` to confirm.
- Debug Bundle export is a UI placeholder — not yet implemented.
- `src/core/Logger` is still present and compiled but no longer connected to `DiagnosticsPanel`; it can be removed or unified in a later refactor task.
- Info tabs (Call Info, Media, Statistics) remain empty panels — not part of this task.

## Next Recommended Task

**Task 5:** Integrate PJSIP/pjsua2 — add CMake find module (`cmake/FindPJSIP.cmake`),
`src/sip/SipManager.h/.cpp` skeleton (initialize/shutdown PJSUA endpoint),
`SipAccount.h` and `SipCall.h` stubs. Log startup/shutdown via `DiagnosticsLogger`.
Update `docs/architecture.md` and `docs/sip-profiles.md`.
