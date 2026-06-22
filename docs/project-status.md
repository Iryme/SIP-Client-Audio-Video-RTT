# Project Status

Last updated: 2026-06-22
Current task count: 3 of N

## Completed Tasks

| # | Task | Branch | Status |
|---|---|---|---|
| 1 | Project skeleton, GUI layout, documentation | feature/project-skeleton | IMPLEMENTED |
| 6 | SIP profile model, persistence, GUI placeholders | feature/sip-profile-manager | IMPLEMENTED |
| 7 | Secure credential storage (CredentialStore, Windows Credential Manager) | feature/secure-credential-storage | IMPLEMENTED |

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
| Status bar | IMPLEMENTED | Placeholder labels |
| Logger (core) | IMPLEMENTED | Levels, categories, signals |
| AppSettings | IMPLEMENTED | QSettings wrapper |
| SipProfile model | IMPLEMENTED | All fields, URI derivation |
| SipProfileManager | IMPLEMENTED | CRUD, validation, persistence, active selection, credential helpers |
| CredentialStore | IMPLEMENTED | Windows Credential Manager backend; MemoryCredentialBackend for tests |
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
| Profile editor dialog | NOT STARTED | |
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
- Profile editor dialog (which would call `setProfilePassword`) is not yet implemented
- No credential migration or export path (by design — credentials are not application data)

## Next Recommended Task

**Task 2 (revised):** Integrate PJSIP/pjsua2 — add CMake find module, `SipManager` skeleton,
`SipAccount`/`SipCall` stubs. Wire `SipProfileManager` → `SipManager` for account config.
Wire `CredentialStore` → `SipManager` to provide auth credentials at registration time.
No real registration yet — just initialize the PJSUA endpoint and log startup/shutdown.
Update `docs/architecture.md`.
