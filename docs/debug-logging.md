# Debug Logging

## Overview

The diagnostics logging system is implemented in `src/diagnostics/DiagnosticsLogger`.
It is the canonical logging foundation for all modules: SIP, SDP, Media, RTT, LMPE, ETSI, and App.

The GUI-facing signal emitter (`src/core/Logger`) wraps the Qt signal layer on top of this module.

---

## Log Levels

| Level | Default | Purpose |
|---|---|---|
| INFO  | ON  | Normal operational events |
| WARN  | ON  | Recoverable issues, degraded state |
| ERROR | ON  | Failures, unexpected conditions |
| DEBUG | OFF | Developer tracing, verbose flow |
| RAW   | OFF | Full SIP/SDP/RTP/T.140 payloads |

**RAW must require explicit user activation. It must never be enabled accidentally.**
- It is disabled in `DiagnosticsLogger` constructor and has no code path that enables it automatically.
- The DiagnosticsPanel UI requires the user to explicitly click the RAW toggle button.

---

## Log Categories

| Category | Value | Description |
|---|---|---|
| APP      | `LogCategory::App`      | Application lifecycle, settings |
| SIP      | `LogCategory::Sip`      | SIP signaling, registration, calls |
| SDP      | `LogCategory::Sdp`      | SDP negotiation, codec selection |
| MEDIA    | `LogCategory::Media`    | Audio/video stream events |
| RTT      | `LogCategory::Rtt`      | RFC 4103 RTT session events |
| LMPE     | `LogCategory::Lmpe`     | LMPE messaging events |
| ETSI     | `LogCategory::Etsi`     | ETSI module events |
| PLATFORM | `LogCategory::Platform` | OS-level, device, network events |

---

## Log Entry Format (plain text export)

```
[YYYY-MM-DD hh:mm:ss.zzz] [LEVEL   ] [CATEGORY] message | payload
```

Payload is omitted from the line when empty.

---

## Security: Automatic Redaction

`DiagnosticsLogger::log()` passes all message and payload strings through a redaction filter
before storing the entry. The filter detects the following sensitive key names
(case-insensitive) followed by `=` or `:` and replaces the value with `***`:

| Sensitive key | Example input | Stored as |
|---|---|---|
| `password`      | `password=hunter2`       | `password=***`      |
| `passwd`        | `passwd: s3cr3t`         | `passwd=***`        |
| `secret`        | `secret=abc123`          | `secret=***`        |
| `token`         | `token=Bearer xyz`       | `token=***`         |
| `authorization` | `Authorization: Basic …` | `authorization=***` |
| `private key`   | `private key=KEYDATA`    | `private key=***`   |
| `auth`          | `auth=myvalue`           | `auth=***`          |

**The `authorization` pattern is matched before `auth`** to avoid partial matching.

Additional rules enforced at the application layer:
- Passwords, SIP authentication secrets, and private keys must never appear in any log at any level.
- Debug bundles exported from the application must sanitize authentication headers.
- RAW SIP traces include Via, Contact, From, To headers but must strip `Authorization` and `Proxy-Authorization`.

---

## DiagnosticsLogger API

### Include

```cpp
#include "diagnostics/DiagnosticsLogger.h"
```

### Singleton access

```cpp
DiagnosticsLogger &logger = DiagnosticsLogger::instance();
```

### Logging

```cpp
logger.info (LogCategory::Sip,   "Registered successfully");
logger.warn (LogCategory::Media, "Packet loss above threshold");
logger.error(LogCategory::App,   "Settings file corrupted");
logger.debug(LogCategory::Rtt,   "T.140 buffer flush");
logger.raw  (LogCategory::Sip,   "INVITE received", rawSipText);

// Full form with payload:
logger.log(LogLevel::Info, LogCategory::Sip, "message", "optional payload");
```

### Level control

```cpp
logger.setLevelEnabled(LogLevel::Debug, true);
bool active = logger.isLevelEnabled(LogLevel::Raw); // false by default
```

### Entry retrieval

```cpp
QList<LogEntry> all      = logger.entries();
QList<LogEntry> sipOnly  = logger.entriesForCategory(LogCategory::Sip);
QList<LogEntry> warns    = logger.entriesForLevel(LogLevel::Warn);
int count                = logger.entryCount();
```

### Management

```cpp
logger.clear();              // remove all stored entries
logger.setMaxEntries(5000);  // cap in-memory buffer (default: 10 000; oldest dropped)
```

### Export

```cpp
// Plain text (one entry per line)
QString text = logger.exportAsText();

// JSON-ready list of maps (serialize with QJsonDocument)
QList<QVariantMap> rows = logger.exportAsJsonReady();
// Each map keys: "timestamp", "level", "category", "message", "payload"
```

---

## Thread Safety

All public methods lock an internal `QMutex`. Logging from PJSIP callbacks,
media threads, or Qt worker threads is safe without additional synchronization.

---

## Memory Management

The buffer is capped at 10 000 entries (configurable via `setMaxEntries()`).
When the cap is reached the oldest entry is removed before the new one is stored.
This prevents unbounded growth during long calls with DEBUG or RAW enabled.

---

## DiagnosticsPanel UI

- Level toggle buttons (INFO WARN ERROR DEBUG RAW) — persistent state via `AppSettings`.
- Category filter (planned — not yet implemented).
- Search field (planned — not yet implemented).
- Clear, Copy Selected, Export Visible, Export Bundle buttons.
- Log table: Time | Level | Category | Message | Payload.
- Color coding: INFO=blue, WARN=yellow, ERROR=red, DEBUG=grey, RAW=dark grey.

---

## Persistence

Log level on/off states are saved via `AppSettings::setLogLevelEnabled()` using `QSettings`.
The log buffer itself is in-memory only and is not persisted across restarts.
Use the Export function to save logs to a file before closing the application.

---

## Source Files

| File | Purpose |
|---|---|
| `src/diagnostics/LogLevel.h`         | `LogLevel` enum |
| `src/diagnostics/LogCategory.h`      | `LogCategory` enum |
| `src/diagnostics/LogEntry.h`         | `LogEntry` struct |
| `src/diagnostics/DiagnosticsLogger.h` | Public API |
| `src/diagnostics/DiagnosticsLogger.cpp` | Implementation |
| `tests/diagnostics/DiagnosticsLoggerTests.cpp` | Qt Test unit tests |
