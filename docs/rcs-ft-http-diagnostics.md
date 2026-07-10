# RCS FT HTTP Diagnostics (Task W095)

Cross-reference: [messaging-diagnostics.md](messaging-diagnostics.md) (the
overall Messaging Diagnostics feature this extends),
[content-encoding-diagnostics.md](content-encoding-diagnostics.md) (the
Content-Encoding decode pipeline this parser sits behind), and
[windows-trace-json-export.md](windows-trace-json-export.md) (the interop
JSON export schema this adds a `rcsFileTransfer` object to).

## What this is

Linphone (and other RCS-capable UAs) can send a SIP MESSAGE whose body is
`application/vnd.gsma.rcs-ft-http+xml` — a small XML "file-info" descriptor
(GSMA RCC.07 / OMA CPM) that points at an HTTP file transfer, rather than
the file itself. This client adds a **read-only diagnostic parser** for
that descriptor: it extracts the metadata the descriptor declares, and does
nothing else with it.

## Explicitly out of scope (never implemented)

- The referenced URL is **never fetched**. No network request of any kind
  is made on account of this feature.
- The file is **never downloaded, saved, or auto-opened**.
- No thumbnail or preview image is **ever auto-displayed**.
- No MSRP session, no real file transfer, no media playback.

This is metadata-only diagnostics, matching the same "diagnostic and
opt-in, never a real transport" posture as the rest of the Messaging
Diagnostics feature (CPIM/IMDN/is-composing/SDP-MSRP).

## What is parsed (`RcsFtHttpParser` / `RcsFtHttpInfo`)

`src/sip/RcsFtHttpParser.h/.cpp` uses `QXmlStreamReader` (the same
non-validating, tolerant approach as `ImdnParser`/`IsComposingParser`) to
extract, from the primary `<file-info>` entry (the one whose `type`
attribute is not `"thumbnail"`, or the first one if `type` is absent):

| `RcsFtHttpInfo` field | Source XML |
|---|---|
| `fileInfoType` | `<file-info type="...">` attribute |
| `fileSize` | `<file-size>` (bytes, `-1` if absent) |
| `fileName` | `<file-name>` |
| `contentType` | `<content-type>` |
| `dataUrl` | `<data url="...">` attribute — full URL, see redaction below |
| `expiresAt` | `<data until="...">` attribute, verbatim |
| `thumbnailPresent` | `true` if a second `<file-info type="thumbnail">` entry exists |
| `disposition` | `<file-disposition>`/`<disposition>`, when present |
| `playingLength` | `<playing-length>`, when present (audio/video duration hint) |

A malformed or incomplete document never throws or crashes the caller — it
simply yields a partially populated (or, for an empty/non-XML body,
entirely absent — `RcsFtHttpInfo::present == false`) result. This satisfies
the task's "parsing must be safe and tolerant" requirement the same way the
IMDN/is-composing parsers already do.

Detection: `MessagingContentKindDetector` recognizes
`application/vnd.gsma.rcs-ft-http+xml` as `MessagingContentKind::RcsFtHttp`;
`MessagingEvent::PayloadType::RcsFtHttp` maps to the JSON/text string
`"rcs-ft-http"`.

## URL redaction (`UrlRedactor`)

The file-transfer URL (`<data url="...">`) commonly carries a one-time
download token or session identifier — in the query string, or sometimes
folded into the path. Per the task's requirement, that token must never
appear in a UI preview or an export by default.

`src/sip/UrlRedactor.h/.cpp` (`UrlRedactor::redact()`) produces:

```
<scheme>://<host>[:<port>][/<first-path-segment>[/…+<8-hex-char fingerprint>]]
```

- **Scheme, host, and port** are kept verbatim — an operator can see
  *where* the file would come from.
- **Query parameters are always dropped**, unconditionally — this is where
  a token/nonce is most likely to live.
- **Only the first path segment is kept as-is.** Any further path segments,
  plus the (now-dropped) query string, are folded into a short SHA-256
  fingerprint (`…+xxxxxxxx`, 8 hex chars) instead of being shown — so the
  same source URL always redacts to the same value (useful for correlating
  repeated transfers of the same file across a trace), without exposing the
  token itself.
- A malformed/non-absolute input (not parseable as a URL with a scheme and
  host) is redacted to `redacted:xxxxxxxx` — a fingerprint of the whole
  opaque string — rather than ever being echoed back verbatim.

### Decision: full URL is not exposed at all by default

The task allows *optionally* keeping the full URL "if current diagnostic
policy permits". No such opt-in policy/setting exists yet in this client
(`AppSettings` has no flag for it), so this implementation takes the
conservative default: **the full, un-redacted URL
(`RcsFtHttpInfo::dataUrl`) is kept only on the in-memory
`MessagingTraceEntry`/`MessagingEvent` structs** (available to a future
opt-in UI/export path if one is added) but **every UI display and every
export path (`InteropTraceExporter`, `MessagingEventStore::exportToJson`/
`exportToText`, `MessagingDiagnosticsStore::exportToJson`/`exportToText`,
and the `MessagingMessageDetailsDialog`) always shows/exports the redacted
form only.** Adding an explicit "show full URL" opt-in is left for a future
task if a real need for it is confirmed.

## UI (`MessagingDiagnosticsPage` / `MessagingMessageDetailsDialog`)

- The messaging feed's payload-type filter includes `rcs-ft-http`.
- The per-message detail dialog (`MessagingMessageDetailsDialog`) shows a
  read-only "RCS FT HTTP" section: file-info type, file name, Content-Type,
  file size, expiration, thumbnail presence, and the **redacted** URL. No
  download button, no image preview — read-only text only, per the task's
  explicit "not in this task" instruction.

## Export (`InteropTraceExporter`, schema v2)

```jsonc
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

Only present when the event's `payloadType` is `rcs-ft-http`. See
[windows-trace-json-export.md](windows-trace-json-export.md) for the full
schema and the `schemaVersion` 1 → 2 migration note.
