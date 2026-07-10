# Windows Messaging/MSRP Diagnostics JSON Export (Interop)

**Task W094** — Added in branch `feature/w094-windows-trace-json-export`, built
on top of [Messaging Diagnostics (W090)](messaging-diagnostics.md),
[Messaging Event Store (W091)](messaging-event-store.md), and
[SIP MESSAGE Foundation (W092)](sip-message.md).
**Fix (branch `fix/w092-imdn-deflate-decoding`)** — added `contentEncoding`,
`decodedBodyPreview`, `parseStatus`, and `parseWarnings` event fields (see
below) so `Content-Encoding: deflate` bodies (and their decode
success/failure) are visible in this export; see
[messaging-diagnostics.md](messaging-diagnostics.md#content-encoding-deflate-srcsipdeflatedecoderhcpp)
for the decoding itself.

## Overview

This task adds a second JSON export format to Messaging Diagnostics,
alongside the existing `MessagingEventStore::exportToJson()` (Task W091):
an **interop-compatible** export intended to be diffable against a trace
exported by the SIP-Server-RTT project's
`scripts/interop/compare-client-server-trace.py`, so a SIP MESSAGE/CPIM/
IMDN/is-composing/MSRP-SDP exchange can be compared client-side vs.
server-side.

**This repository does not contain `scripts/interop/compare-client-server-trace.py`** —
it was not present in this repo at the time this task was implemented (confirmed
by search; no `scripts/interop/` directory exists here). Per the task's own
fallback instruction, this document specifies the schema from Task W094's
field list directly, states every naming choice made where that list was
ambiguous, and ships a worked sample export (see below and
`docs/samples/windows-trace-export-sample.json`) so a future integration can
diff against something concrete instead of only prose.

Strictly diagnostic, same as every other Messaging Diagnostics feature:
exporting never opens an MSRP socket, never negotiates an MSRP session, and
never changes call/media behavior. It only serializes data already captured
by the existing (unmodified) W090 pipeline.

## No new parsing

`InteropTraceExporter` (`src/sip/InteropTraceExporter.h/.cpp`) reads
directly from `MessagingTraceEntry` objects already built by
`MessagingDiagnosticsStore::buildEntry()` (Task W090) — the same structures
`MessagingEventStore` (Task W091) and the existing `exportToJson()`/
`exportToText()` already use. `transport`/`payloadType`/`status`
classification is obtained by calling
`MessagingEventStore::mapFromTraceEntry()` (a `static`, side-effect-free
method) rather than re-deriving it. No CPIM/IMDN/is-composing/SDP-MSRP
parsing is duplicated anywhere in this task.

## Schema

```
{
  "schemaVersion": 1,
  "source": "windows-client",
  "exportedAt": "<ISO-8601 UTC timestamp of the export itself>",
  "events": [ { ...one object per MessagingTraceEntry... } ]
}
```

### Common / per-event fields (Task W094 requirement 2)

| Requirement's field name | JSON key | Source |
|---|---|---|
| schemaVersion | `schemaVersion` (root) | `InteropTraceExporter::kSchemaVersion` (currently `1`) |
| source | `source` (root) | literal `"windows-client"` |
| exportedAt | `exportedAt` (root) | export wall-clock time, UTC, ISO 8601 with milliseconds |
| eventId | `eventId` | 1-based position in the exported list (same convention `MessagingEventStore` already uses for its own ids) |
| timestamp | `timestamp` | `MessagingTraceEntry::timestamp`, ISO 8601 with milliseconds |
| direction | `direction` | `"inbound"` / `"outbound"` (via `MessagingEvent::directionToString`) |
| status | `status` | see "On the meaning of `status`" below |
| transport | `transport` | `"sip-message"` / `"msrp"` / `"unknown"` (via `MessagingEvent::transportToString`) |
| payloadType | `payloadType` | `"plain"` / `"html"` / `"cpim"` / `"imdn"` / `"is-composing"` / `"sdp"` / `"unknown"` |
| Call-ID | `callId` | `MessagingTraceEntry::callId` |
| CSeq | `cseq` | `MessagingTraceEntry::cSeq` |
| From | `from` | `MessagingTraceEntry::fromUri` |
| To | `to` | `MessagingTraceEntry::toUri` |
| Content-Type | `contentType` | `MessagingTraceEntry::contentType` (outer Content-Type header) |
| (fix: deflate decoding) | `contentEncoding` | `MessagingTraceEntry::contentEncoding` — raw `Content-Encoding` header value (e.g. `"deflate"`); empty string if the header was absent |
| bodyPreview | `bodyPreview` | Same capped, single-line preview the diagnostics table already shows (decoded content when `contentEncoding` was successfully reversed) |
| (fix: deflate decoding) | `decodedBodyPreview` | `MessagingTraceEntry::decodedBodyPreview` — same value as `bodyPreview` when decoding applied and succeeded (or there was nothing to decode); empty when decoding was attempted and failed |
| (fix: deflate decoding) | `parseStatus` | Alias of `status` (see below) under the literal field name requested by the deflate-decoding fix — same value, both kept for backward compatibility |
| (fix: deflate decoding) | `parseWarnings` | `MessagingEvent::parseWarnings` array (e.g. `["deflate decode failed"]`); previously only surfaced in the UI table's tooltip, now also in this export |
| rawSipRedacted | `rawSipRedacted` | `MessagingTraceEntry::rawSip` — already credential-redacted by `SipTraceLogger` upstream; this exporter performs no redaction of its own. Never rewritten by deflate decoding — always the original, still-encoded wire bytes |

**Naming convention chosen**: all JSON keys use `camelCase` (`callId`, not
`Call-ID`), matching the style already established by
`MessagingEventStore::exportToJson()` and the sample block in the task
prompt itself (which shows `callId`, `contentType`, `rawSipRedacted`). The
requirement list's own field names (`Call-ID`, `Content-Type`) read as
human-readable protocol header names, not literal JSON key names.

**On the meaning of `status`**: the requirement list gives no further
detail on what "status" should contain. This export uses the diagnostics
pipeline's own `parseStatus` classification (`"ok"` / `"partial"` /
`"error"` — Task W091's `MessagingEvent::parseStatus`, "could this event be
fully interpreted"), since that is the only existing per-event status
concept in this codebase that applies uniformly to *every* event type
(SIP MESSAGE, CPIM, IMDN, is-composing, MSRP-SDP alike). This is explicitly
**not** the same thing as Task W093's outbound send-status
(`queued`/`submitted`/`sent`/`failed`) — that concept only applies to a
live send action taken by this client, not to a captured/replayed trace
entry (which may be inbound, or outbound but observed only via the wire
capture), so it does not fit a generic per-event field here.

### Messaging fields (requirement 3)

Included only when the corresponding structured info was detected
(`*Info::present == true`, same convention as every other export in this
codebase):

```
"messageId": "<top-level shortcut, present only when imdn.present>",
"imdn": {
  "messageId": "...",
  "originalRecipient": "...",
  "finalRecipient": "...",
  "disposition": "delivered" | "displayed" | "failed" | "error" | "none"
},
"cpim": {
  "from": "...",
  "to": "...",
  "dateTime": "...",
  "subject": "...",
  "contentType": "..."          // the CPIM-wrapped body's own Content-Type
},
"isComposing": {
  "state": "active" | "idle" | "gone",
  "timeout": "...",             // non-standard, string as captured
  "refresh": "..."              // RFC 3994 refresh interval in seconds, string as captured
}
```

`timeout`/`refresh` are exported as JSON strings, not numbers — they are
stored as `QString` in `IsComposingInfo` (the parser does not validate they
are numeric; RFC 3994 allows implementations to be lenient here), so this
export does not silently coerce/round-trip them through a numeric type.

### MSRP/SDP fields (requirement 4)

Included only when `SdpMsrpInfo::present == true` (an `m=message` SDP line
was detected):

```
"msrp": {
  "sessionId": "...",           // best-effort, parsed from the last a=path MSRP URI
  "mediaLine": "m=message 12345 TCP/MSRP *",   // the raw m=message SDP line
  "path": "...",                // a=path value
  "acceptTypes": "...",         // a=accept-types value
  "setup": "...",               // a=setup value
  "connection": "...",          // a=connection value
  "transportProtocol": "TCP/MSRP" | "TCP/TLS/MSRP"
}
```

**`transaction_id`, `From-Path`, `To-Path` are intentionally omitted.**
Those are per-chunk MSRP protocol headers (RFC 4975), only meaningful
inside a live MSRP session's own chunk framing. This client only ever
performs **SDP-level** MSRP diagnostics (detecting that an `m=message`
offer/answer was exchanged) — it never opens a real MSRP socket or
transaction (see [msrp-diagnostics.md](msrp-diagnostics.md)), so there is
no MSRP transaction or per-chunk header to report. `path` (from the SDP
`a=path` attribute) is the closest available substitute and is already
included.

## UI

Messaging Diagnostics gained a new **"Export Interop JSON"** button
alongside the existing "Export Text" and "Export JSON" buttons (which are
unchanged — `MessagingEventStore`'s own JSON export, Task W091, keeps its
original shape for backward compatibility with anything already consuming
it). The new button has a tooltip naming
`scripts/interop/compare-client-server-trace.py` so its purpose is
discoverable in the UI itself. Nothing was added to or near call control —
this page has none to begin with.

## Sample export

A minimal worked example, following the task prompt's own sample shape,
extended with this export's additional common fields:

```json
{
  "schemaVersion": 1,
  "source": "windows-client",
  "exportedAt": "2026-07-09T21:00:00.000Z",
  "events": [
    {
      "eventId": 1,
      "timestamp": "2026-07-09T20:59:59.500Z",
      "direction": "inbound",
      "status": "ok",
      "transport": "sip-message",
      "payloadType": "cpim",
      "callId": "a84b4c76e66710",
      "cseq": "1 MESSAGE",
      "from": "sip:alice@example.com",
      "to": "sip:bob@example.com",
      "contentType": "message/cpim",
      "bodyPreview": "Wheee!",
      "messageId": "",
      "cpim": {
        "from": "MR SANDERS <im:piglet@example.com>",
        "to": "Depressed Donkey <im:eeyore@example.com>",
        "dateTime": "2000-12-13T13:40:00-08:00",
        "subject": "the weather will be fine today",
        "contentType": "text/plain; charset=utf-8"
      },
      "rawSipRedacted": "MESSAGE sip:bob@example.com SIP/2.0\r\nContent-Type: message/cpim\r\n\r\nFrom: MR SANDERS <im:piglet@example.com>\r\nTo: Depressed Donkey <im:eeyore@example.com>\r\nDateTime: 2000-12-13T13:40:00-08:00\r\nSubject: the weather will be fine today\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nWheee!"
    }
  ]
}
```

The full, machine-generated sample (also covering IMDN, is-composing, and
SDP MSRP events) lives at
[`docs/samples/windows-trace-export-sample.json`](samples/windows-trace-export-sample.json)
and is validated by `tests/test_windows_trace_json_export.cpp`
(`sampleExportIsValidJson`, `requiredFieldsPresent`).

## Compatibility with `compare-client-server-trace.py`

Not verified end-to-end, since the script is not present in this
repository. What this task *does* guarantee:
- Every field explicitly named in the W094 requirement list has a
  documented JSON key above, with an explicit note wherever the mapping
  required a judgment call (naming convention, `status` semantics,
  omitted MSRP transaction-level fields).
- The output is valid JSON (`QJsonDocument::fromJson` round-trips
  cleanly — verified by test).
- The schema is versioned (`schemaVersion: 1`) so a future breaking change
  on either side has an explicit place to signal it.

If/when `compare-client-server-trace.py` becomes available in this
repository, the next step is a direct diff between its expected schema and
this document — any mismatch should be resolved by adjusting
`InteropTraceExporter`, not by working around it downstream.

## Tests (`tests/test_windows_trace_json_export.cpp`)

Pure Qt, no PJSIP dependency.

| Test | Covers |
|---|---|
| `exportSipMessageBasic` | Common fields for a plain SIP MESSAGE event |
| `exportCpimFields` | `cpim` object (from/to/dateTime/subject/contentType) |
| `exportImdnFields` | `imdn` object + top-level `messageId` shortcut |
| `exportIsComposingFields` | `isComposing` object (state/timeout/refresh) |
| `exportSdpMsrpFields` | `msrp` object (sessionId/mediaLine/path/acceptTypes/setup/connection/transportProtocol) |
| `rawSipRedacted` | Authorization header value never appears in `rawSipRedacted`; `[REDACTED]` marker present |
| `sampleExportIsValidJson` | Output parses via `QJsonDocument::fromJson` with no error; root fields present |
| `requiredFieldsPresent` | All Task W094 requirement-2 event fields present as JSON keys |
| `exportIncludesDeflateDecodingFields` | `contentEncoding`/`decodedBodyPreview`/`parseStatus`/`parseWarnings` for both a valid deflate-encoded IMDN body (decoded, `parseStatus="ok"`) and an invalid one (`parseStatus="partial"`, `"deflate decode failed"` in `parseWarnings`, empty `decodedBodyPreview`) |

Run:

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_windows_trace_json_export
ctest --test-dir build_tests -R test_windows_trace_json_export --output-on-failure
```

All 9 test functions pass locally, alongside the full existing suite
(43/43 total — see [project-status.md](project-status.md)).

## What remains diagnostic-only

- MSRP: same as every prior messaging task — no session ever opened,
  negotiated, or accepted. This export only reads already-detected SDP
  attributes.
- Exporting is a pure read of in-memory state; it has no effect on any live
  call, registration, or messaging behavior.

## What is NOT implemented

- No verified compatibility with the actual
  `compare-client-server-trace.py` script (not present in this repo).
- No automatic/scheduled export — this is a manual, on-demand UI action
  only (same as the existing Export Text/Export JSON buttons).
- No streaming/incremental export — each click serializes the store's full
  current snapshot.
- No schema migration tooling — `schemaVersion` is present but nothing
  currently reads/reacts to it on the client side (forward-looking field
  only, per the requirement).
