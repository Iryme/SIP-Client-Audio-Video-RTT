# Architecture

## Overview

Layered Qt 6 C++ application. The GUI layer is strictly separated from the SIP stack and media engine to allow independent testing and future backend replacement.

```
┌─────────────────────────────────────────────────────┐
│                     GUI Layer                        │
│  MainWindow → NavRail, Sidebar, CallPanel,           │
│               VideoPanel, RttPanel, Diagnostics      │
├─────────────────────────────────────────────────────┤
│                 Application Core                     │
│  Logger, AppSettings, AppController (future)         │
├─────────────────────────────────────────────────────┤
│               SIP Stack (planned)                    │
│  pjsua2 wrapper — SipAccount, SipCall, SipTransport  │
├──────────────────┬──────────────────────────────────┤
│  Media Engine    │  RTT Engine        ETSI Modules   │
│  (planned)       │  (planned)         (optional)     │
│  AudioDevice     │  RttSession        Ts103479       │
│  VideoDevice     │  T140Buffer        Ts103480       │
│  RtpStream       │                    Ts103698Lmpe   │
└──────────────────┴──────────────────────────────────┘
```

## Module Boundaries

| Module | Location | Status |
|---|---|---|
| GUI panels | `src/gui/` | IMPLEMENTED (skeleton) |
| Logger | `src/core/Logger` | IMPLEMENTED |
| AppSettings | `src/core/AppSettings` | IMPLEMENTED |
| SIP stack | `src/sip/` | NOT STARTED |
| Media engine | `src/media/` | NOT STARTED |
| RTT engine | `src/rtt/` | NOT STARTED |
| ETSI modules | `src/etsi/` | NOT STARTED |

## Key Design Decisions

- GUI runs entirely on the Qt main thread.
- All SIP/media operations will run on background threads or Qt async patterns.
- The UI thread must never block on network or media I/O.
- ETSI modules are compiled conditionally via CMake options.
- `Logger` is a singleton with thread-safe `emit` via Qt queued connections.

## Dependency Graph (current)

```
main.cpp
  └── Application
        └── MainWindow
              ├── NavRail
              ├── SidebarPanel
              ├── CallPanel
              ├── VideoPanel
              │     └── (future: Qt Multimedia video surface)
              ├── RttPanel
              ├── DiagnosticsPanel ──► Logger (signal)
              └── AppStatusBar
```

## Planned SIP Integration (PJSIP / pjsua2)

- `src/sip/SipManager` — owns PJSUA endpoint, account list, call list
- `src/sip/SipAccount` — wraps `pj::Account`
- `src/sip/SipCall` — wraps `pj::Call`
- All callbacks post events to the Qt main thread via `QMetaObject::invokeMethod`

## Settings Persistence

- Uses `QSettings` in INI format under user scope.
- `AppSettings` provides typed accessors.
- Secrets (passwords) use platform keychain (planned), never INI.
