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
| SipProfileEditorDialog | `src/gui/dialogs/` | IMPLEMENTED |
| MediaPanel | `src/gui/panels/MediaPanel` | IMPLEMENTED |
| Logger | `src/core/Logger` | IMPLEMENTED |
| AppSettings | `src/core/AppSettings` | IMPLEMENTED |
| SipProfile model | `src/sip/SipProfile.h` | IMPLEMENTED |
| SipProfileManager | `src/sip/SipProfileManager` | IMPLEMENTED |
| CredentialStore | `src/security/CredentialStore` | IMPLEMENTED |
| WindowsCredentialBackend | `src/security/` | IMPLEMENTED |
| MemoryCredentialBackend | `src/security/` | IMPLEMENTED (tests only) |
| MediaDevice model | `src/media/MediaDevice.h` | IMPLEMENTED |
| MediaDeviceManager | `src/media/MediaDeviceManager` | IMPLEMENTED |
| MediaDeviceSelectionModel | `src/media/MediaDeviceSelectionModel` | IMPLEMENTED |
| QtMediaDeviceBackend | `src/media/` | IMPLEMENTED |
| SIP stack (PJSIP) | `src/sip/` | NOT STARTED |
| Media streaming | `src/media/` | NOT STARTED (device enum done) |
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
              ├── SidebarPanel ──► SipProfileManager (singleton)
              ├── CallPanel
              ├── VideoPanel
              │     └── (future: Qt Multimedia video surface)
              ├── RttPanel
              ├── DiagnosticsPanel ──► Logger (signal)
              └── AppStatusBar

SipProfileManager ──► Logger (Sip category)
SipProfileManager ──► QSettings (SIPClientProfiles.ini, UserScope)
SipProfileManager ──► CredentialStore (credential helpers)

CredentialStore ──► Logger (Platform category)
CredentialStore ──► WindowsCredentialBackend ──► Advapi32 (Windows Credential Manager)

MediaPanel ──► MediaDeviceSelectionModel ──► MediaDeviceManager ──► QtMediaDeviceBackend
MediaDeviceSelectionModel ──► AppSettings (media/device/*)
MediaDeviceManager ──► Logger (Media category)
```

## Secure Credential Layer (Task 7)

- `src/security/ICredentialBackend` — pure interface for pluggable backends
- `src/security/CredentialStore` — singleton, owns the active backend, provides typed API
- `src/security/WindowsCredentialBackend` — Windows Credential Manager via Advapi32
- `src/security/MemoryCredentialBackend` — in-memory backend for unit tests (not secure)
- Passwords never written to QSettings, SipProfile, or Logger
- Backend injected via `setBackend()` for test isolation

## SIP Profile Layer (Task 6)

- `src/sip/SipProfile` — struct: all profile fields except credentials
- `src/sip/SipProfileManager` — CRUD, validation, persistence, active selection
- Profile data persisted to `SIPClientProfiles.ini` (separate from app prefs)
- Passwords deferred to OS keychain — never in QSettings (ADR-009)

## Media Device Layer (Task 9)

- `src/media/MediaDevice` — struct: id, displayName, type, isDefault, isAvailable
- `src/media/IMediaDeviceBackend` — pure interface; `QtMediaDeviceBackend` wraps `QMediaDevices`
- `src/media/MediaDeviceManager` — singleton, enumerates all device types, owns the backend
- `src/media/MediaDeviceSelectionModel` — handles persistence (`AppSettings`) and fallback to default
- `src/gui/panels/MediaPanel` — GUI widget mounted in the "Media" info tab
- No real audio/video capture yet — device enumeration only

## Planned SIP Integration (PJSIP / pjsua2)

- `src/sip/SipManager` — owns PJSUA endpoint, account list, call list
- `src/sip/SipAccount` — wraps `pj::Account`, reads from `SipProfileManager`
- `src/sip/SipCall` — wraps `pj::Call`
- All callbacks post events to the Qt main thread via `QMetaObject::invokeMethod`

## Settings Persistence

- Uses `QSettings` in INI format under user scope.
- `AppSettings` provides typed accessors.
- Secrets (passwords) use platform keychain (planned), never INI.
