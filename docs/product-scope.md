# Product Scope

## Purpose

A generic, cross-platform SIP multimedia client for desktop use. Supports audio calls, video calls, and real-time text (RTT). Designed to work with standard SIP servers. ETSI emergency-calling extensions are optional and modular.

## This is NOT an emergency-only client

The primary product is a general-purpose SIP softphone. Emergency and accessibility extensions (RFC 4103, ETSI TS 103 479, 103 480, 103 698) are optional modules that do not affect the base product behavior.

## Supported Platforms

- Windows 10/11 (x64)
- Linux: Ubuntu 22.04+, Fedora 38+ (x64)

## Minimum Usable Resolution

1024 × 768

## Core Features

| Feature | Priority | Status |
|---|---|---|
| SIP profile management | P1 | NOT STARTED |
| SIP registration (UDP/TCP/TLS) | P1 | NOT STARTED |
| Audio calls | P1 | NOT STARTED |
| Video calls | P2 | NOT STARTED |
| RFC 4103 RTT over RTP/T.140 | P2 | NOT STARTED |
| LMPE messaging panel | P2 | NOT STARTED |
| Diagnostics and log panel | P1 | IMPLEMENTED |
| Debug bundle export | P2 | NOT STARTED |
| Media device selection | P1 | NOT STARTED |
| Call information panel | P2 | NOT STARTED |
| SIP/media/RTT statistics | P3 | NOT STARTED |

## Optional ETSI Modules

| Module | Status |
|---|---|
| ETSI TS 103 479 | NOT STARTED |
| ETSI TS 103 480 | NOT STARTED |
| ETSI TS 103 698 LMPE | NOT STARTED |

ETSI modules must be compiled as separate optional components. The base SIP client must build and run without any ETSI dependencies.

## Security Requirements

- Passwords are never stored in clear text.
- Passwords, private keys, tokens are never included in debug bundles or log exports.
- RAW log level is disabled by default and requires explicit user activation.
- SIP TLS transport must be supported (implementation pending).
