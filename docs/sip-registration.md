# SIP Registration

**Status:** NOT STARTED

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

## Implementation Notes

- PJSIP `Account::onRegState()` callback → post update to Qt main thread → update sidebar + status bar.
- Registration events are logged at `INFO` level.
- Registration errors logged at `WARN` or `ERROR`.
- Authentication challenges logged at `DEBUG`.
- Authorization headers are never logged at any level.
