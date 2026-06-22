# Project Status

Last updated: 2026-06-22
Current task count: 2 of N

## Completed Tasks

| # | Task | Branch | Status |
|---|---|---|---|
| 1 | Project skeleton, GUI layout, documentation | feature/project-skeleton | IMPLEMENTED |
| 5 | Persistent application settings (ApplicationSettings, DiagnosticsPanel integration, tests) | feature/persistent-settings | IMPLEMENTED |

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
| Diagnostics/log panel | IMPLEMENTED | Level states persisted via ApplicationSettings |
| Status bar | IMPLEMENTED | Placeholder labels |
| Logger (core) | IMPLEMENTED | Levels, categories, signals |
| ApplicationSettings | IMPLEMENTED | QSettings INI, typed accessors, defaults, reset |
| AppSettings (shim) | IMPLEMENTED | Delegates to ApplicationSettings |
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

## Known Limitations (Task 5)

- Category filter and search in DiagnosticsPanel are persisted keys but the UI filter-on-type is not yet wired to the table
- Window state (`ui/state` — QMainWindow dock/toolbar state) is stored in ApplicationSettings but not yet called from MainWindow (geometry and splitters are persisted; full QMainWindow::restoreState not yet used)
- Theme selection persists the string but there is no theme-switch UI — always loads `dark_theme.qss` at startup regardless of stored value
- Device IDs (microphone, speaker, camera) are persisted but no device selection UI exists yet
- SIP profile ID is persisted but no SIP profile UI exists yet
- No credential storage implementation — passwords remain deferred to the SIP profile task (ADR-009)

## Known Limitations (Task 1)

- No SIP networking — application is GUI-only
- No PJSIP dependency — must be added in the next SIP task
- Nav rail buttons have text labels only (no icons)
- Contact list uses hardcoded placeholder entries
- Info tabs (Call Info, Media, Statistics) are empty panels
- Video area shows a placeholder paintEvent, no real video
- Local preview is a labeled box, no camera input

## Next Recommended Task

**Task 2:** Integrate PJSIP/pjsua2 — add CMake find module, SipManager skeleton, SipAccount, SipCall stubs. No real calls yet — just initialize the PJSUA endpoint and log startup/shutdown. Update `docs/architecture.md` and `docs/sip-profiles.md`.
