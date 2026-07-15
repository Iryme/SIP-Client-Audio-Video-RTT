# Windows Messaging/MSRP Diagnostics JSON Export (Interop)

**Task W094** — Added in branch `feature/w094-windows-trace-json-export`, built
on top of [Messaging Diagnostics (W090)](messaging-diagnostics.md),
[Messaging Event Store (W091)](messaging-event-store.md), and
[SIP MESSAGE Foundation (W092)](sip-message.md).
**Task W095** — Extended in branch `feature/w095-deflate-rcs-diagnostics`:
bumped `schemaVersion` 1 → 2, adding Content-Encoding decode diagnostics and
an optional `rcsFileTransfer` object to every event — see
[content-encoding-diagnostics.md](content-encoding-diagnostics.md) and
[rcs-ft-http-diagnostics.md](rcs-ft-http-diagnostics.md).
**Task W096** — Extended in branch `feature/w096-imdn-foundation`: adds
`generatedImdn`/`receivedImdn`/`correlatedMessageId`/`deliveryState` to
every event — purely additive, `schemaVersion` stays `2`. See
[imdn.md](imdn.md).
**Task W097** — Extended in branch `feature/w097-is-composing`: adds
`generatedIsComposing`/`receivedIsComposing`/`typingState`/`typingRefresh`/
`typingTimeout` to every event — purely additive, `schemaVersion` stays
`2`. See [is-composing.md](is-composing.md).
**Task W098** — Extended in branch `feature/w098-presence-foundation`: adds
a new top-level `presenceEvents` array, entirely independent of the
`events` array documented below — SUBSCRIBE/NOTIFY/PIDF traces are never
mixed into the SIP MESSAGE event stream. A new top-level key is additive by
definition, so `schemaVersion` stays `2`. See [presence.md](presence.md).
**Task W099** — Extended in branch `feature/w099-xcap-foundation`: adds a
new top-level `xcapEvents` array, entirely independent of both `events` and
`presenceEvents` — XCAP is plain HTTP, not SIP, so its diagnostics never
mix with either SIP-trace-based array. Again purely additive, so
`schemaVersion` stays `2`. See [xcap.md](xcap.md).
**Task W102** — Extended in branch `feature/w102-msrp-live-interoperability`:
bumps `schemaVersion` 2 → 3. Every `msrpSessions` entry gains `role`
(active-connector/passive-listener, real — set by which of
`MsrpSession::connectAsActive`/`listenAsPassive` actually ran),
`remoteSetup`, `negotiationState` (the previously-unexported
`offerAnswerState`), and a `peerAssociation` object
`{method, mediaIndex, sipHeaderCallId, confidence}`. All v2 fields are
unchanged. See [msrp-live-interoperability.md](msrp-live-interoperability.md)
for what else W102 did and did not add to the schema (msrpTransactions/
fallbackEvents/interopValidation/tlsDiagnostics were considered and
deliberately deferred — see that document's "Not yet added" note).
**Task W100** — Extended in branch `feature/w100-msrp-foundation`: adds two
new top-level arrays, `msrpSessions` (from `MsrpSessionStore`) and
`msrpEvents` (from `MsrpDiagnosticsStore`), plus four additive fields on
every existing `events` entry — `selectedTransport`, `actualTransport`,
`fallbackUsed`, `fallbackReason` (currently always `"sip-message"`/
`"sip-message"`/`false`/empty, since per-message MSRP transport selection
is not yet wired into the live send path — see
[msrp-foundation.md](msrp-foundation.md) §12). No existing field's meaning
changes, so `schemaVersion` stays `2`. See [msrp-foundation.md](msrp-foundation.md)
and [msrp-protocol.md](msrp-protocol.md).

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
  "schemaVersion": 3,
  "source": "windows-client",
  "exportedAt": "<ISO-8601 UTC timestamp of the export itself>",
  "events": [ { ...one object per MessagingTraceEntry, now with selectedTransport/actualTransport/fallbackUsed/fallbackReason, Task W100... } ],
  "presenceEvents": [ { ...one object per PresenceTraceEntry, Task W098... } ],
  "xcapEvents": [ { ...one object per XcapResult, Task W099... } ],
  "msrpSessions": [ { ...one object per MsrpSessionInfo, Task W100... } ],
  "msrpEvents": [ { ...one object per MsrpDiagnosticsEvent, Task W100... } ]
}
```

### `presenceEvents` (Task W098)

Built from `PresenceDiagnosticsStore`'s `PresenceTraceEntry` rows
(SUBSCRIBE/NOTIFY raw-trace diagnostics — see [presence.md](presence.md)),
entirely independent of `events`/`MessagingTraceEntry`. Each entry:

| JSON key | Source |
|---|---|
| `eventId` | 1-based position in the exported presence list |
| `timestamp` | trace timestamp, ISO 8601 with milliseconds |
| `direction` | `"inbound"` / `"outbound"` |
| `method` | `"SUBSCRIBE"` or `"NOTIFY"` |
| `callId` | `Call-ID` header |
| `cseq` | `CSeq` header |
| `from` / `to` | `From` / `To` header values, verbatim |
| `eventPackage` | `Event` header value (expected `"presence"`) |
| `subscriptionState` | `Subscription-State` header's state token (pending/active/terminated), lowercased |
| `subscriptionExpires` | `expires=` param on `Subscription-State`, else the `Expires` header; `-1` if neither present |
| `subscriptionReason` | `reason=` param on `Subscription-State`, normalized via `PresenceInfo::normalizeSubscriptionReason` |
| `contentType` | `Content-Type` header |
| `parseStatus` / `parseWarnings` | from PIDF parsing (`PidfParser`), `"ok"`/`"partial"`/`"error"` |
| `rawSipRedacted` | full raw SIP text, already redacted upstream by `SipTraceLogger` |
| `presence.entity` / `.tupleId` / `.basicStatus` / `.extendedStatus` / `.contact` / `.priority` / `.note` / `.timestamp` | parsed PIDF fields (empty/`"unknown"` when the body was absent or not `application/pidf+xml`) |

### `xcapEvents` (Task W099)

Built from `XcapDiagnosticsStore`'s `XcapResult` rows (completed
GET/PUT/DELETE/HEAD operations — see [xcap.md](xcap.md)), entirely
independent of both `events` and `presenceEvents` — XCAP is plain HTTP,
never drawn as SIP traffic. Each entry:

| JSON key | Source |
|---|---|
| `eventId` | 1-based position in the exported XCAP list |
| `method` | `"GET"` / `"PUT"` / `"DELETE"` / `"HEAD"` |
| `timestamp` | operation completion timestamp, ISO 8601 with milliseconds |
| `duration` | request duration in milliseconds |
| `urlRedacted` | full document URL, redacted via `XcapUrlRedactor` (userinfo/tokens/sensitive query dropped; scheme/host/port/AUID/partial selector kept) |
| `auid` | the document's AUID (e.g. `resource-lists`, `pres-rules`) |
| `xui` | effective XUI used (document-level override, else server config) |
| `selector` | document name/selector |
| `nodeSelector` | optional RFC 4825 node selector, if any |
| `status` | HTTP status code (`0` if the request never reached the network — e.g. a validation failure) |
| `contentType` | response `Content-Type` |
| `contentLength` | response body length in bytes |
| `etag` / `lastModified` | response `ETag` / `Last-Modified` headers |
| `parseStatus` | `"n/a"` / `"ok"` / `"partial"` / `"error"` — XML validation classification for GET responses; PUT/DELETE/HEAD are always `"n/a"` |
| `warnings` | XML validation warnings, if any |
| `networkError` | `true` if the request failed at the transport level (timeout, connection refused, TLS failure, ...) |
| `errorString` | present only when `networkError` is `true` |

### `msrpSessions` / `msrpEvents` (Task W100)

`msrpSessions` — one object per `MsrpSessionInfo` (from `MsrpSessionStore`):
`eventId`, `sessionKey`, `sipCallId`, `localSessionId`, `remoteSessionId`,
`localPathRedacted`/`remotePathRedacted` (transport label only — no raw
path), `transport`, `setup`, `connection`, `direction`, `acceptTypes`,
`negotiated`/`connected`/`established` (booleans, distinct per task
requirement C — a session is never `established` merely because SDP
negotiation looked complete), `state`, `createdAt`/`updatedAt`/
`connectedAt`/`closedAt`, `bytesSent`/`bytesReceived`/`framesSent`/
`framesReceived`/`messagesCompleted`, `warnings`, `lastError`.

`msrpEvents` — one object per `MsrpDiagnosticsEvent` (from
`MsrpDiagnosticsStore`): `eventId`, `timestamp`, `direction`, `sessionKey`,
`transactionId`, `messageId`, `method`, `responseCode`, `statusHeader`,
`toPathRedacted`/`fromPathRedacted`, `contentType`, `byteRange`,
`continuation`, `bodyPreview`, `bodyLength`, `rawFrameRedacted`,
`transport`, `parseStatus`, `warnings`.

Both arrays are independent of `events`/`presenceEvents`/`xcapEvents` —
MSRP is plain TCP protocol traffic, never drawn as SIP traffic.

### schemaVersion 1 → 2 migration (Task W095)

`schemaVersion` was bumped from `1` to `2`. **No v1 field was removed or
renamed** — every field documented below as "requirement 2/3/4" is
unchanged. v2 only *adds*:
- `parseStatus` (alias of `status`, same value — see the literal
  `parseStatus` field the task asked for, kept alongside `status` rather
  than replacing it) and `parseWarnings` (array, previously only on
  `MessagingEventStore`'s own export, not this one).
- `contentEncoding` / `decodeStatus` / `decodeVariant` /
  `compressedBodyLength` / `decodedBodyLength` / `decodeError` (see
  [content-encoding-diagnostics.md](content-encoding-diagnostics.md)).
- An optional `rcsFileTransfer` object, present only when
  `payloadType == "rcs-ft-http"` (see
  [rcs-ft-http-diagnostics.md](rcs-ft-http-diagnostics.md)).

A v1-only consumer that ignores unrecognized JSON keys continues to work
unmodified against a v2 export; a consumer that wants the new decode/RCS
diagnostics needs to be updated to read the v2-only fields.

### Common / per-event fields (Task W094 requirement 2)

| Requirement's field name | JSON key | Source |
|---|---|---|
| schemaVersion | `schemaVersion` (root) | `InteropTraceExporter::kSchemaVersion` (currently `3` — bumped by Task W102, see migration note above) |
| source | `source` (root) | literal `"windows-client"` |
| exportedAt | `exportedAt` (root) | export wall-clock time, UTC, ISO 8601 with milliseconds |
| eventId | `eventId` | 1-based position in the exported list (same convention `MessagingEventStore` already uses for its own ids) |
| timestamp | `timestamp` | `MessagingTraceEntry::timestamp`, ISO 8601 with milliseconds |
| direction | `direction` | `"inbound"` / `"outbound"` (via `MessagingEvent::directionToString`) |
| status | `status` | see "On the meaning of `status`" below |
| parseStatus | `parseStatus` (v2) | literal alias of `status`, same value — added because Task W095 (and W092 before it) asked for a field literally named `parseStatus` |
| parseWarnings | `parseWarnings` (v2) | array of human-readable warning strings, e.g. `"deflate decode failed"` |
| transport | `transport` | `"sip-message"` / `"msrp"` / `"unknown"` (via `MessagingEvent::transportToString`) |
| payloadType | `payloadType` | `"plain"` / `"html"` / `"cpim"` / `"imdn"` / `"is-composing"` / `"sdp"` / `"rcs-ft-http"` (v2) / `"unknown"` |
| Call-ID | `callId` | `MessagingTraceEntry::callId` |
| CSeq | `cseq` | `MessagingTraceEntry::cSeq` |
| From | `from` | `MessagingTraceEntry::fromUri` |
| To | `to` | `MessagingTraceEntry::toUri` |
| Content-Type | `contentType` | `MessagingTraceEntry::contentType` (outer Content-Type header) |
| bodyPreview | `bodyPreview` | Same capped, single-line preview the diagnostics table already shows — the **decoded** body when Content-Encoding was present, never raw compressed bytes |
| contentEncoding | `contentEncoding` (v2) | Raw `Content-Encoding` header value, verbatim; empty when absent |
| decodedBodyPreview | `decodedBodyPreview` (v2) | Same as `bodyPreview` — kept as its own field name per the task's literal field list |
| decodeStatus | `decodeStatus` (v2) | `"not-needed"` / `"decoded"` / `"failed"` / `"unsupported"` / `"limit-exceeded"` — see [content-encoding-diagnostics.md](content-encoding-diagnostics.md) |
| decodeVariant | `decodeVariant` (v2) | `"zlib"` / `"raw-deflate"` / `"gzip"` / `"none"` |
| compressedBodyLength | `compressedBodyLength` (v2) | Size (bytes) of the raw body handed to the decoder |
| decodedBodyLength | `decodedBodyLength` (v2) | Size (bytes) of the decoded output, when `decodeStatus == "decoded"` |
| decodeError | `decodeError` (v2) | Human-readable failure reason; **key omitted entirely** (not even an empty string) when there is nothing to report |
| rawSipRedacted | `rawSipRedacted` | `MessagingTraceEntry::rawSip` — already credential-redacted by `SipTraceLogger` upstream; this exporter performs no redaction of its own. **Unchanged by Task W095** — still the original wire bytes, compressed body included |
| generatedImdn | `generatedImdn` (v2, additive) | `true` when this event's body is an outbound IMDN report (Task W096) |
| receivedImdn | `receivedImdn` (v2, additive) | `true` when this event's body is an inbound IMDN report (Task W096) |
| correlatedMessageId | `correlatedMessageId` (v2, additive) | `ImdnInfo::messageId` — the original message this report is about; empty when this event is not an IMDN report |
| deliveryState | `deliveryState` (v2, additive) | `"delivered"` / `"displayed"` / `"failed"` / `"error"` / `"forbidden"` / `"processed"` / `"none"` — `ImdnInfo::dispositionToString(...)`, empty-string when not an IMDN report |
| generatedIsComposing | `generatedIsComposing` (v2, additive) | `true` when this event's body is an outbound is-composing notification (Task W097) |
| receivedIsComposing | `receivedIsComposing` (v2, additive) | `true` when this event's body is an inbound is-composing notification (Task W097) |
| typingState | `typingState` (v2, additive) | `"active"` / `"idle"` / `"gone"` / `"unknown"` — `IsComposingInfo::stateToString(...)`, empty-string when not an is-composing notification |
| typingRefresh | `typingRefresh` (v2, additive) | RFC 3994 `<refresh>` value, verbatim |
| typingTimeout | `typingTimeout` (v2, additive) | non-standard `<timeout>` value, verbatim |

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

### RCS FT HTTP fields (Task W095)

Included only when `payloadType == "rcs-ft-http"` (`RcsFtHttpInfo::present == true`):

```
"rcsFileTransfer": {
  "fileInfoType": "file",
  "fileName": "photo.jpg",
  "fileSize": 204800,
  "contentType": "image/jpeg",
  "dataUrlRedacted": "https://files.example.test/dl/…+3a1f9c02",
  "expiresAt": "2030-01-01T00:00:00.000Z",
  "thumbnailPresent": true
}
```

**`dataUrlRedacted` is always the redacted form** — query parameters/tokens
stripped, only the first path segment kept verbatim, the rest folded into a
short fingerprint (`UrlRedactor`). The full un-redacted URL is never
exported. See [rcs-ft-http-diagnostics.md](rcs-ft-http-diagnostics.md) for
the full parser and redaction policy.

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
  "schemaVersion": 3,
  "source": "windows-client",
  "exportedAt": "2026-07-09T21:00:00.000Z",
  "events": [
    {
      "eventId": 1,
      "timestamp": "2026-07-09T20:59:59.500Z",
      "direction": "inbound",
      "status": "ok",
      "parseStatus": "ok",
      "parseWarnings": [],
      "transport": "sip-message",
      "payloadType": "cpim",
      "callId": "a84b4c76e66710",
      "cseq": "1 MESSAGE",
      "from": "sip:alice@example.com",
      "to": "sip:bob@example.com",
      "contentType": "message/cpim",
      "bodyPreview": "Wheee!",
      "contentEncoding": "",
      "decodedBodyPreview": "Wheee!",
      "decodeStatus": "not-needed",
      "decodeVariant": "none",
      "compressedBodyLength": 0,
      "decodedBodyLength": 0,
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
(`sampleExportIsValidJson`, `requiredFieldsPresent`). It predates the v2
schema bump (Task W095) and was not regenerated — it is still valid v2
output (every v1 field it shows is unchanged), just without the new v2-only
fields illustrated in the inline sample above.

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
| `exportContentEncodingDecodedFields` (Task W095) | `contentEncoding`/`decodeStatus`/`decodeVariant`/`compressedBodyLength`/`decodedBodyLength`/`decodedBodyPreview` for a successfully decoded deflate IMDN body |
| `exportContentEncodingFailedFields` (Task W095) | `decodeStatus == "failed"`, `decodeError` present, `parseStatus == "partial"`, `"deflate decode failed"` in `parseWarnings`, no `imdn` object emitted |
| `exportRcsFileTransferFields` (Task W095) | `rcsFileTransfer` object fields, `dataUrlRedacted` never contains the source token |

Run:

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_windows_trace_json_export
ctest --test-dir build_tests -R test_windows_trace_json_export --output-on-failure
```

All 11 test functions pass locally, alongside the full existing suite
(46/46 total — see [project-status.md](project-status.md)).

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
- **Task W095**: the RCS FT HTTP transfer URL is never fetched by this
  exporter or anything upstream of it — only the redacted form is ever
  written to the export file.
