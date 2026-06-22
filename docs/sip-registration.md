# SIP Registration

**Status:** NOT STARTED (PJSIP endpoint lifecycle implemented in Task 10 — accounts/registration deferred)

## Overview

SIP registration announces the client's location to the SIP registrar so that incoming calls can be routed correctly.

## Planned Behavior

- Registration triggered on profile activation or application start.
- Re-registration before expiry (configurable, default: 60 s before expiry).
- Deregistration on profile deactivation or clean shutdown.
- Registration failure causes a retry with exponential backoff.

## Status Indicators

| State | Color | Sidebar Label |
|---|---|---|
| Registered | Green | ● Registered |
| Registering | Yellow | ● Registering... |
| Not registered | Red | ● Not registered |
| Registration failed | Red | ● Registration failed |
| Deregistering | Grey | ● Deregistering... |

## Status Bar

The bottom status bar shows the global connection state and active account URI when registered.

## Current State (Task 10)

`SipManager` owns the PJSIP endpoint lifecycle:

```
Application::Application() → SipManager::initialize()
Application::~Application() → SipManager::shutdown()
```

When `ENABLE_PJSIP=OFF` (default): stub mode — endpoint is not created, manager reports "Stub SIP backend".
When `ENABLE_PJSIP=ON` and PJSIP found: `pj::Endpoint::libCreate()` / `libInit()` / `libStart()` are called at startup; `libDestroy()` at shutdown.

No `SipAccount` or `SipCall` objects are created yet — that is the next step.

## Implementation Notes (future)

- PJSIP `Account::onRegState()` callback → post update to Qt main thread → update sidebar + status bar.
- Registration events are logged at `INFO` level.
- Registration errors logged at `WARN` or `ERROR`.
- Authentication challenges logged at `DEBUG`.
- Authorization headers are never logged at any level.
