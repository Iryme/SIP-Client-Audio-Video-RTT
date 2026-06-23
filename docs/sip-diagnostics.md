# SIP Diagnostics & SIP Ladder

**Task 20** — Added in branch `feature/sip-diagnostics-ladder`.

## Overview

Task 20 adds a SIP message trace store and a ladder diagram view to the Diagnostics panel.  In stub mode (default, `ENABLE_PJSIP=OFF`) synthetic SIP traces are emitted by `SipManager` at key registration and call state transitions.  When PJSIP is available, traces can be extended to carry real status codes and URIs from `onRegState` and `onCallState` callbacks (see Known Limitations).

## Architecture

```
SipManager / SipAccount / SipCall
          │  call logMessage() at key transitions
          ▼
    SipTraceLogger  (singleton, Qt-only)
          │  emits messageLogged(SipMessageTrace)
          ▼
    SipLadderWidget  (paintEvent-based ladder in DiagnosticsPanel)
    DiagnosticsPanel / SIP Ladder tab  (export Text / JSON buttons)
```

### `SipMessageTrace` (`src/sip/SipMessageTrace.h`)

Plain POD struct with `Q_DECLARE_METATYPE`:

| Field | Type | Purpose |
|---|---|---|
| `timestamp` | `QDateTime` | Set automatically if not provided |
| `direction` | `Direction` | `Inbound` or `Outbound` |
| `method` | `QString` | SIP method (REGISTER, INVITE, BYE, ACK, CANCEL, …) |
| `statusCode` | `int` | 0 for requests; 1xx–6xx for responses |
| `statusText` | `QString` | "OK", "Trying", "Ringing", "Unauthorized", etc. |
| `fromUri` | `QString` | From/source URI |
| `toUri` | `QString` | To/destination URI |
| `callId` | `QString` | Call-ID header value |
| `cSeq` | `QString` | CSeq header value (e.g., "1 REGISTER") |
| `rawSip` | `QString` | Full SIP message text; **Authorization headers are replaced with [REDACTED]** before storage |

`summary()` returns the method for requests and `"statusCode statusText"` for responses.

### `SipTraceLogger` (`src/sip/SipTraceLogger.h/.cpp`)

Application-layer singleton. Qt-only (no `pjsua2.hpp`).

- `logMessage(trace)` — redacts credentials, sets timestamp, appends to list, logs `rawSip` at `LogLevel::Raw` (off by default), emits `messageLogged`.
- `messages()` — read-only access to the ordered list.
- `clear()` — empties list, emits `cleared`.
- `exportToText()` — plain text with direction arrows `>>>` / `<<<`.
- `exportToJson()` — JSON array; no `rawSip` included (credentials protection).

### Credential Redaction

`SipTraceLogger::redactCredentials()` replaces the value portion of any `Authorization:` or `Proxy-Authorization:` header with `[REDACTED]`.  This runs unconditionally on every stored `rawSip`, even in test environments.

### `SipLadderWidget` (`src/gui/SipLadderWidget.h/.cpp`)

Custom `QWidget` placed in a `QScrollArea` inside the "SIP Ladder" tab.

- `paintEvent()` draws two entity columns ("Local (UA)" and "Remote") with dashed timeline lines and horizontal arrows for each trace.
- Arrow direction: outbound = left→right; inbound = right→left.
- Color coding:

| Color | Meaning |
|---|---|
| Blue `#5B8CFF` | REGISTER / UNREGISTER |
| Green `#5EBF6C` | INVITE |
| Red `#FF6B6B` | BYE |
| Orange `#FF9944` | CANCEL / 4xx client error |
| Gray `#AAAAAA` | 1xx provisional |
| Green `#50D890` | 2xx success |
| Red `#FF5555` | 5xx/6xx error |

- Each row shows timestamp (left margin), method/status on the arrow, and optional CSeq / Call-ID below the arrow.
- `sizeHint()` / `minimumSizeHint()` grow with message count so the `QScrollArea` enables scrolling automatically.

### `DiagnosticsPanel` additions

The existing single-tab layout was replaced with a `QTabWidget`:

- **Tab 0 "Log"** — unchanged; existing table, level filters, search, Clear/Copy/Export/Bundle buttons.
- **Tab 1 "SIP Ladder"** — `SipLadderWidget` in a `QScrollArea` with Clear, Export Text, Export JSON buttons.  The "Bundle" button in Tab 0 was updated to reference the SIP Ladder export.

### `SipManager` stub trace emission

Key emission points (all in `SipManager.cpp`; no `HAVE_PJSIP` guard required — both paths call them):

| Event | Direction | Method / Status |
|---|---|---|
| `registerActiveProfile()` | Outbound | REGISTER |
| `onAccountRegistrationStateChanged(Registered)` | Inbound | 200 OK |
| `onAccountRegistrationStateChanged(RegistrationFailed)` | Inbound | error code |
| `unregisterActiveProfile()` | Outbound | REGISTER (Expires: 0) |
| `onAccountRegistrationStateChanged(Unregistered)` | Inbound | 200 OK |
| `makeCall()` | Outbound | INVITE |
| `onAccountIncomingCall()` | Inbound | INVITE |
| `onActiveCallStateChanged(Ringing)` | Inbound | 180 Ringing |
| `onActiveCallStateChanged(Active)` | Inbound + Outbound | 200 OK + ACK |
| `onActiveCallStateChanged(Disconnecting)` | Outbound | BYE |
| `onActiveCallStateChanged(Failed, code≥400)` | Inbound | error code |

## Diagnostics

Trace summaries are logged by `Logger::raw()` when a `rawSip` body is present (RAW level, off by default).  Info-level logging for SIP signalling already exists in `SipManager` and `SipCall`; the ladder supplements rather than duplicates it.

## Tests (`tests/test_sip_trace.cpp`)

6 Qt Test cases (`QTEST_GUILESS_MAIN`). All inject traces directly via `SipTraceLogger::instance().logMessage()`.

| Test | Description |
|---|---|
| `registerTrace` | REGISTER request stored; method, direction, timestamp correct |
| `inviteTrace` | INVITE request stored; callId and summary correct |
| `byeTrace` | BYE request stored; direction Outbound |
| `directionInboundOutbound` | Both directions preserved; summary formats correct |
| `authorizationRedacted` | `rawSip` with Authorization + Proxy-Authorization — values replaced with [REDACTED]; non-sensitive headers preserved |
| `clearAndExport` | `exportToText` contains arrows and methods; `exportToJson` contains expected keys; `clear()` emits `cleared` signal and empties list |

## Known Limitations

- PJSIP path: traces in `SipManager` emit in both stub and PJSIP builds, but PJSIP callbacks (`onRegState`, `onCallState`) could provide richer information (e.g., actual registrar URI, real call-ids). Low-level PJSIP module hooking for byte-accurate raw SIP capture is not implemented.
- Call direction (incoming vs. outgoing) is not tracked by `SipManager`, so the 200 OK on `Active` is always marked Inbound in stub mode. This may differ for incoming calls.
- Raw SIP capture (`rawSip` field) requires the caller to populate it. `SipManager` stub traces leave `rawSip` empty; full capture requires a PJSIP logging module.
- The SIP Ladder export does not include `rawSip` in JSON output (credential-safe by design).
- Auto-scroll uses a queued `invokeMethod` which may occasionally miss the final position before the scroll bar range updates.
