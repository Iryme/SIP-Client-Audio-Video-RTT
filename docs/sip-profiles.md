# SIP Profiles

**Status:** NOT STARTED

## Overview

A SIP profile (account) contains the configuration needed to register with a SIP server and make/receive calls.

## Planned Fields per Profile

| Field | Required | Notes |
|---|---|---|
| Profile name | Yes | Display only |
| SIP URI | Yes | sip:user@domain |
| Registrar | Yes | SIP server hostname or IP |
| Username | Yes | Auth username |
| Password | Yes | Stored in OS keychain, never in plain text |
| Transport | Yes | UDP / TCP / TLS |
| SIP port | No | Default 5060 (UDP/TCP), 5061 (TLS) |
| Registration expiry | No | Default 3600 s |
| STUN server | No | For NAT traversal |
| TURN server | No | For relay |
| Outbound proxy | No | Optional proxy route |

## Storage

- Profiles stored in `QSettings` INI file (user scope).
- Passwords stored in OS keychain via a future `SecretStore` abstraction.
- Profile list: `accounts/count`, `accounts/N/*` keys.

## UI Location

Settings dialog → Accounts tab (not yet implemented).

## Implementation Notes

- Each profile maps to a `pjsua2::AccountConfig` when SIP integration is added.
- Multiple simultaneous profiles must be supported.
- The active/default profile is shown in the sidebar account card.
