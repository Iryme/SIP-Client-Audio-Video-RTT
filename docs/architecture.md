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
| DiagnosticsLogger | `src/diagnostics/` | IMPLEMENTED |
| Logger (GUI adapter) | `src/core/Logger` | IMPLEMENTED |
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
- `DiagnosticsLogger` is the canonical logging foundation — all modules use it directly.
- `Logger` (in `src/core/`) is a thin Qt-signal adapter on top of DiagnosticsLogger for GUI delivery.
- DiagnosticsLogger is thread-safe via `QMutex`; safe to call from PJSIP threads without extra sync.

## DiagnosticsLogger Design

`src/diagnostics/DiagnosticsLogger` is the single in-memory log store for the application lifetime.

Key properties:
- **Not a QObject** — no Qt event loop dependency; callable from any thread.
- **Automatic redaction** — sensitive key patterns (`password`, `token`, `authorization`, `private key`, etc.) are replaced with `***` before storage.
- **Bounded buffer** — default 10 000 entries; oldest dropped when cap is reached.
- **Export** — plain text (human-readable) and JSON-ready (`QList<QVariantMap>`) formats.
- **Category filtering** — `entriesForCategory()` / `entriesForLevel()` return filtered views.
- **Clear** — `clear()` wipes in-memory buffer; does not affect exported files.

The GUI adapter (`src/core/Logger`) forwards calls to DiagnosticsLogger and also emits
`entryAdded(LogEntry)` Qt signal so `DiagnosticsPanel` can update the table in real time.

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
