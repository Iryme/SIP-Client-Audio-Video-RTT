# Debug Logging

## Log Levels

| Level | Default | Purpose |
|---|---|---|
| INFO | ON | Normal operational events |
| WARN | ON | Recoverable issues, degraded state |
| ERROR | ON | Failures, unexpected conditions |
| DEBUG | OFF | Developer tracing, verbose flow |
| RAW | OFF | Full SIP/SDP/RTP/T.140 payloads |

RAW must require explicit user activation. It must never be enabled accidentally.

## Log Categories

| Category | Description |
|---|---|
| APP | Application lifecycle, settings |
| SIP | SIP signaling, registration, calls |
| SDP | SDP negotiation, codec selection |
| MEDIA | Audio/video stream events |
| RTT | RFC 4103 RTT session events |
| LMPE | LMPE messaging events |
| ETSI | ETSI module events |
| PLATFORM | OS-level, device, network events |

## Log Entry Format

```
[hh:mm:ss.zzz] [LEVEL] [CATEGORY] message
```

Payload (optional) is shown in the Payload column only.

## Security Rules

- Passwords, SIP authentication secrets, and private keys must never appear in any log at any level.
- Debug bundles exported from the application must sanitize authentication headers before packaging.
- RAW SIP traces include Via, Contact, From, To headers but must strip Authorization and Proxy-Authorization.

## Logger API

```cpp
// Singleton access
Logger &log = Logger::instance();

// Convenience methods
log.info (LogCategory::Sip,  "Registered successfully");
log.warn (LogCategory::Media,"Packet loss above threshold");
log.error(LogCategory::App,  "Settings file corrupted");
log.debug(LogCategory::Rtt,  "T.140 buffer flush");
log.raw  (LogCategory::Sip,  "INVITE received", rawSipText);

// Enable/disable at runtime
log.setLevelEnabled(LogLevel::Debug, true);
```

## DiagnosticsPanel UI

- Level toggle buttons (INFO WARN ERROR DEBUG RAW) — persistent state.
- Category filter (future — not yet implemented).
- Search field (future — not yet implemented).
- Clear, Copy Selected, Export Visible, Export Bundle buttons.
- Log table: Time | Level | Category | Message | Payload.
- Color coding: INFO=blue, WARN=yellow, ERROR=red, DEBUG=grey, RAW=dark grey.

## Persistence

Log level on/off states are persisted via `ApplicationSettings` (`src/settings/ApplicationSettings`).

- On `DiagnosticsPanel` construction, each level toggle reads its last-saved state via `ApplicationSettings::instance().diagLevelEnabled(level)`. If no value is stored, the canonical default is returned (INFO/WARN/ERROR on, DEBUG/RAW off).
- When the user toggles a level button, the new state is saved immediately via `ApplicationSettings::instance().setDiagLevelEnabled(level, on)`.
- **RAW** is never enabled by default or after a reset. A corrupt/unrecognised stored value for RAW falls back safely to `false`.
- Log contents (the entries shown in the table) are never persisted — only the level enable/disable state is saved.

Keys stored:
```
diagnostics/level/info   = true/false
diagnostics/level/warn   = true/false
diagnostics/level/error  = true/false
diagnostics/level/debug  = true/false
diagnostics/level/raw    = true/false
```

See [docs/application-settings.md](application-settings.md) for the full settings reference.
