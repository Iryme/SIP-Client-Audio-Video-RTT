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
| DiagnosticsLogger | `src/diagnostics/DiagnosticsLogger` | IMPLEMENTED — QObject with signal (Task 3+4) |
| LogFilterModel | `src/diagnostics/LogFilterModel` | IMPLEMENTED — filter/search logic (Task 4) |
| Logger (legacy) | `src/core/Logger` | IMPLEMENTED — kept; not yet removed |
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
- `DiagnosticsLogger` is the canonical logging API — all modules use it directly.
- `DiagnosticsLogger` is thread-safe via `QMutex`; safe to call from PJSIP threads without extra sync.
- `LogFilterModel` isolates filter/search logic from the GUI so it can be unit-tested without a display.
- `Logger` (in `src/core/`) is a legacy standalone class retained for backward compatibility; it is not connected to `DiagnosticsPanel` in Task 4 onwards.

## DiagnosticsLogger Design (updated Task 4)

`src/diagnostics/DiagnosticsLogger` is the single in-memory log store and the primary logging API.

Key properties:
- **QObject singleton** — emits `entryAdded(LogEntry)` after each accepted log call; `DiagnosticsPanel` connects to this signal directly.
- **Thread-safe emit** — mutex is released before `emit entryAdded(entry)` to prevent deadlock; connect from background threads with `Qt::QueuedConnection`.
- **Automatic redaction** — sensitive key patterns (`password`, `token`, `authorization`, `private key`, etc.) are replaced with `***` before storage.
- **Bounded buffer** — default 10 000 entries; oldest dropped when cap is reached.
- **Export** — plain text (human-readable) and JSON-ready (`QList<QVariantMap>`) formats.
- **Category/level filtering** — `entriesForCategory()` / `entriesForLevel()` return filtered views.
- **Clear** — `clear()` wipes in-memory buffer; does not affect exported files.

## LogFilterModel Design

`src/diagnostics/LogFilterModel` decouples filter and search logic from `DiagnosticsPanel`.

- Stores all received `LogEntry` objects (independent of `DiagnosticsLogger`'s own buffer).
- Exposes `setLevelVisible()`, `setCategoryFilter()`, `setSearchText()`, `setRawVisible()`.
- `matchesFilter(entry)` applies all active filters synchronously.
- Emits `filterChanged()` when any filter setter changes a value (no-op if value unchanged).
- `DiagnosticsPanel` calls `rebuildTable()` on `filterChanged` to keep the log table in sync.
- Has no GUI dependency — fully testable with `QTEST_GUILESS_MAIN`.

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
              ├── DiagnosticsPanel ──► DiagnosticsLogger (entryAdded signal)
              │     └── LogFilterModel (filter / search logic)
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
