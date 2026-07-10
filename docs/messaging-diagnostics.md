# Messaging Diagnostics

**Task W090** — Added in branch `feature/w090-msrp-lmpe-diagnostics`.
**Task W091** — Extended in branch `feature/w091-messaging-event-store`: the
UI/export layer now reads from the transport-independent `MessagingEventStore`
instead of talking to `MessagingDiagnosticsStore` directly. See
[messaging-event-store.md](messaging-event-store.md) for the new model.
**Task W092** — Extended in branch `feature/w092-sip-message-foundation`: the
same page gained a "Send SIP MESSAGE" composer, so this feed is no longer
purely passive — it also displays messages this client itself sends. MSRP
remains completely untouched/disabled. See [sip-message.md](sip-message.md)
for the send/receive foundation.
**Task W093** — Extended in branch `feature/w093-incoming-message-history`:
added a dedicated inbound-message callback and a separate "Message History"
table. See [message-history.md](message-history.md).
**Task W094** — Extended in branch `feature/w094-windows-trace-json-export`:
added an "Export Interop JSON" button producing a server-comparable schema
(SIP-Server-RTT `compare-client-server-trace.py`). Purely an additional
export format — no changes to capture/parsing. See
[windows-trace-json-export.md](windows-trace-json-export.md).
**Fix (branch `fix/w092-imdn-deflate-decoding`)** — Linphone sometimes sends
`Content-Encoding: deflate` bodies (e.g. compressed `message/imdn+xml`),
which the diagnostics pipeline previously tried to parse as raw XML,
producing `parseStatus=partial`. Added deflate decoding before CPIM/IMDN/
is-composing parsing runs; see the "Content-Encoding: deflate" section
below.

## Overview

This task adds a read-only diagnostics foundation for SIP instant messaging: SIP
MESSAGE requests/responses, CPIM-wrapped bodies, IMDN disposition notifications,
RFC 3994 is-composing indications, and MSRP-related SDP attributes (`m=message`,
`a=path`, `a=accept-types`, `a=setup`, `a=connection`). It captures, parses,
logs, and displays this traffic — it does **not** implement real MSRP sessions,
and MSRP is never started implicitly. Everything here is strictly diagnostic,
mirroring the existing [SIP Ladder / SIP Diagnostics](sip-diagnostics.md)
feature.

(As of Task W092, this client *can* send/receive basic SIP MESSAGE — see
[sip-message.md](sip-message.md). That capability is layered on top of this
diagnostics pipeline without modifying it: composing a message produces a
synthetic trace that flows through the exact same
`SipTraceLogger → MessagingDiagnosticsStore → MessagingEventStore` path
described below, and receiving one requires no new code at all since
`PjsipTraceModule` already taps every raw SIP message. MSRP itself remains
exactly as described in this document: detection/diagnostics only, never a
real session.)

## Architecture

```
PjsipTraceModule (existing raw SIP capture — unchanged)
          │
          ▼
   SipRawMessageParser::parse()   (existing — unchanged)
          │
          ▼
     SipTraceLogger                (existing singleton — unchanged)
          │  emits messageLogged(SipMessageTrace)
          ▼
MessagingDiagnosticsStore::onSipMessageLogged()
          │  isMessagingRelevant() filters to MESSAGE / CPIM / IMDN /
          │  is-composing / SDP m=message traffic only
          │  buildEntry() parses structured content
          ▼
   MessagingTraceEntry             (base trace fields + parsed CPIM/IMDN/
                                     is-composing/MSRP-SDP structs)
          │  emits entryLogged(MessagingTraceEntry)
          ▼
MessagingEventStore::onDiagnosticsEntryLogged()   (Task W091 — maps into the
          │  transport-independent MessagingEvent shape; no re-parsing)
          ▼
      MessagingEvent                (id, direction, transport, payloadType,
                                      from/to/callId/cseq/contentType,
                                      bodyPreview, rawSipRedacted,
                                      parseStatus, parseWarnings)
          │  emits eventAppended(MessagingEvent)
          ▼
MessagingDiagnosticsPage           ("Messaging" nav page — table + filters
                                     + Clear/Export Text/Export JSON, sourced
                                     from MessagingEventStore)
          │  row double-click (resolves structured detail by id from
          │  MessagingDiagnosticsStore — not duplicated in the UI)
          ▼
MessagingMessageDetailsDialog      (full structured detail view + raw SIP)
```

`MessagingDiagnosticsStore` **never touches PJSIP or SipManager directly** — it
only observes traces that `SipTraceLogger` already captured for the SIP
Ladder. No new pjsua2 callback registration was needed: `PjsipTraceModule`
already captures every raw SIP message (including MESSAGE requests/responses
and re-INVITEs carrying MSRP SDP offers) below the transaction layer.

### Relevance filter (`MessagingDiagnosticsStore::isMessagingRelevant`)

A `SipMessageTrace` is treated as messaging-related if any of:
- `method` is `MESSAGE` (case-insensitive; recovered from CSeq for responses, same as the SIP Ladder).
- `Content-Type` is `message/cpim`, `message/imdn+xml`, or `application/im-iscomposing+xml`.
- The body contains an `m=message` SDP line (e.g. an INVITE/re-INVITE offering MSRP).

All other SIP traffic (REGISTER, INVITE without `m=message`, BYE, ACK, …)
is ignored by this feature — it still appears in the SIP Ladder as before.

### `MessagingTraceEntry` (`src/sip/MessagingTraceEntry.h`)

One row of the Messaging Diagnostics feed:

| Field | Purpose |
|---|---|
| `timestamp`, `direction`, `method`, `statusCode`, `statusText` | Same semantics as `SipMessageTrace` |
| `fromUri`, `toUri`, `callId`, `cSeq`, `contentType` | Copied from the underlying SIP headers |
| `contentEncoding` | Raw `Content-Encoding` header value (e.g. `deflate`); empty if the header was absent |
| `bodyPreview` | Safe, single-line, whitespace-collapsed, length-capped (160 chars) preview for the UI/export table — reflects the **decoded** body when `contentEncoding` was successfully reversed; a display convenience, not a security redaction |
| `decodedBodyPreview` | Same value as `bodyPreview` whenever decoding applied and succeeded (or when there was no encoding to decode); empty when decoding was attempted and failed |
| `contentEncodingDecodeFailed` | `true` when `contentEncoding` named a recognized coding but decoding the body against it failed |
| `rawSip` | Full raw SIP text; Authorization/Proxy-Authorization values are **already redacted** by `SipTraceLogger` before the entry is built. Always the original, still-encoded bytes — decoding never rewrites `rawSip` |
| `contentKind` | `MessagingContentKind` detected from `Content-Type` (independent of `Content-Encoding` — the declared MIME type, not the wire coding) |
| `cpim`, `imdn`, `isComposing`, `sdpMsrp` | Structured info, present only when detected (see below); parsed from the **decoded** body when `Content-Encoding` applies |

### Content-Type detection (`src/sip/MessagingContentKind.h/.cpp`)

`MessagingContentKindDetector::detect()` classifies a `Content-Type` header
into: `PlainText` (`text/plain`), `Html` (`text/html`), `Cpim` (`message/cpim`),
`Imdn` (`message/imdn+xml`), `IsComposing` (`application/im-iscomposing+xml`),
`Sdp` (`application/sdp`), or `Unknown`. Pure string matching on the base
media type (parameters like `;charset=` are stripped first).

### CPIM (`src/sip/CpimParser.h/.cpp`, RFC 3862)

Parses the CPIM header block (`From`, `To`, `DateTime`, `Subject`,
`Content-Type`) and the wrapped body that follows the blank line separator —
same header/body split convention as `SipRawMessageParser`. When a MESSAGE
body's outer `Content-Type` is `message/cpim`, `MessagingDiagnosticsStore`
re-detects the content kind from the **inner** CPIM `Content-Type` and parses
IMDN / is-composing from the CPIM-wrapped body if applicable.

### IMDN (`src/sip/ImdnParser.h/.cpp`, RFC 5438)

`QXmlStreamReader`-based (no `QtXml` module dependency — `QXmlStreamReader`
ships in Qt Core). Detects disposition from `<delivered/>`, `<displayed/>`,
`<failed/>`, or `<error/>` elements, and extracts `<message-id>`,
`<original-recipient>`, and `<final-recipient>` when present.

### is-composing (`src/sip/IsComposingParser.h/.cpp`, RFC 3994)

`QXmlStreamReader`-based. Detects `<state>` (`active`/`idle`/`gone`) and
extracts `<refresh>` and the non-standard `<timeout>` element when present.

### SDP MSRP diagnostics (`src/sip/SdpMsrpDiagnosticsParser.h/.cpp`)

See [msrp-diagnostics.md](msrp-diagnostics.md) for full detail. Summary:
finds the `m=message` media block in an SDP body and extracts
`a=path`, `a=accept-types`, `a=setup`, `a=connection`, the transport
protocol (`TCP/MSRP` or `TCP/TLS/MSRP`), and a best-effort `session-id`
parsed out of the last MSRP URI in `a=path`.

### Content-Encoding: deflate (`src/sip/DeflateDecoder.h/.cpp`)

Some SIP UAs (observed with Linphone) send `message/imdn+xml` (and
potentially CPIM/is-composing) bodies compressed with
`Content-Encoding: deflate`. `DeflateDecoder::decode()` reverses this before
any CPIM/IMDN/is-composing/SDP-MSRP parsing runs, in
`MessagingDiagnosticsStore::buildEntry()`:

1. `SipRawMessageParser::parse()` extracts the raw `Content-Encoding` header
   value into `SipMessageTrace::contentEncoding` (same `findHeaderValue()`
   convention as `Content-Type`).
2. If `contentEncoding` case-insensitively equals `deflate` and the body is
   non-empty, `DeflateDecoder::decode()` is called on the body bytes
   *before* `CpimParser`/`ImdnParser`/`IsComposingParser`/
   `SdpMsrpDiagnosticsParser` ever see it. On success, all downstream
   parsing runs against the decoded text exactly as it would for an
   unencoded body — this is a pre-processing step, not a new parser.
3. `bodyPreview` reflects the decoded content on success (or the raw,
   still-compressed bytes if decoding failed or was never attempted);
   `decodedBodyPreview` mirrors it, but stays empty when decoding was
   attempted and failed. `rawSip` / `rawSipRedacted` are **never** rewritten
   — they always carry the original wire bytes.
4. On decode failure, `MessagingTraceEntry::contentEncodingDecodeFailed` is
   set, CPIM/IMDN/is-composing/SDP-MSRP parsing is skipped for that entry
   (there is nothing valid to parse), and
   `MessagingEventStore::mapFromTraceEntry()` — the single place that
   already turns detection gaps into `parseStatus`/`parseWarnings` — sets
   `parseStatus = Partial` and appends the warning `"deflate decode
   failed"`. This is the same mechanism Task W091 already uses for e.g. "no
   CPIM header block could be parsed", so no new UI code was needed: the
   Messaging Diagnostics table's existing "Parse" column and tooltip, and
   the interop JSON export's `parseWarnings` array (see
   [windows-trace-json-export.md](windows-trace-json-export.md)), already
   surface it.

**Implementation note (why no new dependency was added):** this codebase
does not link zlib directly. `DeflateDecoder` reuses Qt's own zlib linkage
via `qUncompress()` instead: it synthesizes the 4-byte big-endian length
prefix `qUncompress()`'s own `qCompress()` framing expects, wrapping the raw
wire bytes (a bare zlib/RFC 1950 stream — the format the HTTP/SIP "deflate"
content-coding actually specifies, and what `qCompress()` itself produces
after its own 4-byte prefix). The prefix value is only a buffer-size hint —
`qUncompress()` doubles it and retries on its own until it succeeds or zlib
reports real corruption — so an arbitrary value works; only genuinely
invalid/corrupt/truncated streams fail to decode.

**Scope:** the raw `Content-Encoding` header applies to the entire SIP body
for that message, so decoding happens once, before CPIM detection — not
separately for a CPIM-wrapped inner body. If a future UA is found to
compress only the CPIM-wrapped inner part while leaving an outer
`message/cpim` body uncompressed, that would need a second decode point
inside the CPIM-wrapped-body branch; not implemented, as it has not been
observed.

Only `deflate` is recognized. Other `Content-Encoding` values are left
untouched (the body is parsed as-is, same behavior as before this fix) —
`gzip`/`identity`/etc. are not implemented.

### `MessagingDiagnosticsStore` (`src/sip/MessagingDiagnosticsStore.h/.cpp`)

Singleton, same shape as `SipTraceLogger`:
- `entries()` — read-only ordered list.
- `clear()` — empties the list, emits `cleared`.
- `exportToText()` / `exportToJson()` — include base fields, detected
  `contentKind`, `contentEncoding`/`decodedBodyPreview`, and all structured
  CPIM/IMDN/MSRP sections that were detected; `rawSip` is included and is
  already credential-redacted (and, per the above, never rewritten by
  deflate decoding).
- `isMessagingRelevant()` / `buildEntry()` are `static` so they can be (and
  are) unit tested directly without going through the signal chain.

### `MessagingDiagnosticsPage` (`src/gui/panels/MessagingDiagnosticsPage.h/.cpp`)

"Messaging" nav-rail page (own icon, between "SIP Ladder" and "History" —
does not crowd call control). Read-only `QTableWidget` feed sourced from
`MessagingEventStore` (Task W091; see [messaging-event-store.md](messaging-event-store.md))
with columns Time / Direction / Transport / From / To / Call-ID /
Content-Type / Preview / Parse, filters (Call-ID substring, payload type,
direction), and Clear / Export Text / Export JSON / Export Interop JSON
(Task W094 — server-comparable schema, see
[windows-trace-json-export.md](windows-trace-json-export.md)) buttons acting on the
event store. Parse warnings are shown inline in the "Parse" cell (with the
full list in the cell's tooltip) — never as a blocking dialog, so a
malformed body never interrupts the live feed or the UI thread. A visible
banner states the feature is read-only and never starts an MSRP session.
Row double-click resolves the event's structured CPIM/IMDN/is-composing/
SDP-MSRP detail from `MessagingDiagnosticsStore` (by id, not re-parsed) and
opens `MessagingMessageDetailsDialog` (`src/gui/MessagingMessageDetailsDialog.h/.cpp`),
mirroring `SipMessageDetailsDialog`.

Above the feed, a "Send SIP MESSAGE" group (Task W092) lets the user compose
and send a SIP MESSAGE (recipient, Content-Type, body, Enable SIP MESSAGE /
Enable CPIM / Request IMDN checkboxes, Send button, inline status label).
Sent and received messages both appear as rows in the same table below —
see [sip-message.md](sip-message.md) for the full send/receive design.

All parsing happens synchronously inside the Qt slot invoked when
`SipTraceLogger` emits `messageLogged` (already marshaled to the GUI thread by
`PjsipTraceModule`) — pure string/XML parsing on small bodies, no network or
blocking I/O, so the UI thread is never blocked.

### SIP Ladder integration

The SIP Ladder already displayed every SIP message generically (including
MESSAGE), since `PjsipTraceModule`/`SipRawMessageParser` are method-agnostic.
This task adds, without touching capture/parsing:
- A dedicated MESSAGE arrow color (`#B084F5`) in `SipLadderWidget::colorForTrace`.
- A short `[CPIM]` / `[IMDN]` / `[is-composing]` / `[MSRP-SDP]` badge under the
  arrow (`SipLadderWidget.cpp`, local `contentTypeTag()` helper) so these are
  visible in the ladder itself, not only in the new Messaging Diagnostics page.

## Diagnostics

Structured parsing failures are silent (a body that doesn't match the
expected shape simply leaves the corresponding `*Info.present` flag `false`);
this mirrors the existing tolerant, best-effort style of `SipRawMessageParser`.

## Tests

| File | Covers |
|---|---|
| `tests/test_cpim_parser.cpp` | CPIM header parsing, wrapped body extraction, empty/garbled input |
| `tests/test_imdn_parser.cpp` | delivered/displayed/failed dispositions, message-id, original/final recipient |
| `tests/test_is_composing_parser.cpp` | active/idle/gone states, refresh, timeout |
| `tests/test_sdp_msrp_diagnostics_parser.cpp` | TCP/MSRP and TCP/TLS/MSRP media blocks, session-id extraction, non-MSRP SDP rejection |
| `tests/test_deflate_decoder.cpp` | Valid zlib/deflate round-trip (via `qCompress()`), empty input, corrupt/garbage input, UTF-8 payload round-trip |
| `tests/test_sip_raw_message_parser.cpp` | (extended) `Content-Encoding` header extraction, absent-header case |
| `tests/test_messaging_diagnostics_store.cpp` | Relevance filtering, CPIM→IMDN nesting, is-composing, INVITE+MSRP-SDP relevance, clear/export, plain IMDN, valid deflate-encoded IMDN (decoded + parsed), invalid deflate-encoded IMDN (`parseStatus=partial` + `"deflate decode failed"` warning), plain is-composing regression |
| `tests/test_messaging_event_store.cpp` | See [messaging-event-store.md](messaging-event-store.md) |
| `tests/test_sip_message_foundation.cpp` | See [sip-message.md](sip-message.md) |
| `tests/test_message_history.cpp` | See [message-history.md](message-history.md) |
| `tests/test_windows_trace_json_export.cpp` | See [windows-trace-json-export.md](windows-trace-json-export.md) (extended: `contentEncoding`/`decodedBodyPreview`/`parseStatus`/`parseWarnings` for valid and invalid deflate) |

Run (see [build-windows.md](build-windows.md) for full environment setup):

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_cpim_parser test_imdn_parser test_is_composing_parser test_sdp_msrp_diagnostics_parser test_messaging_diagnostics_store test_messaging_event_store test_sip_message_foundation test_message_history test_windows_trace_json_export
ctest --test-dir build_tests -R "test_cpim_parser|test_imdn_parser|test_is_composing_parser|test_sdp_msrp_diagnostics_parser|test_messaging_diagnostics_store|test_messaging_event_store|test_sip_message_foundation|test_message_history|test_windows_trace_json_export" --output-on-failure
```

All 10 suites (W090 + W091 + W092 + W093 + W094 + the deflate-decoding fix)
pass locally.

## Known Limitations

- No real MSRP session is ever started — MSRP diagnostics are detection-only, by design (see [msrp-diagnostics.md](msrp-diagnostics.md)).
- As of Task W092 this client can send/receive basic SIP MESSAGE (plain/HTML/CPIM body, optional IMDN request) — see [sip-message.md](sip-message.md) for the send/receive foundation and its own limitations (no delivery confirmation, no IMDN generation, no MSRP).
- IMDN `original-recipient` / `final-recipient` are not part of the core RFC 5438 `<imdn>` schema (they originate from RFC 8098 email-style disposition notifications); this parser extracts them opportunistically if present but they will usually be absent in a strict RFC 5438 IMDN body.
- CPIM nesting is one level deep: a CPIM body whose inner `Content-Type` is itself `message/cpim` is not recursively unwrapped.
- Only `Content-Encoding: deflate` is decoded; other codings (`gzip`, etc.) are left as-is and will still show up as unparsed/partial. Only a body-level zlib/RFC 1950 stream is recognized — a raw (headerless) RFC 1951 DEFLATE stream, which some non-conformant senders emit instead, will fail to decode (`parseStatus=partial`, `"deflate decode failed"`).
- Deflate decoding is applied once at the outer SIP body; a compressed inner body nested inside an *uncompressed* CPIM wrapper is not supported (not observed in practice).
- `bodyPreview` is a UI truncation convenience, not a redaction pass; the only redaction applied to message content is the existing `SipTraceLogger::redactCredentials()` (Authorization/Proxy-Authorization headers), which runs before a trace ever reaches this feature.
- No persistence — like the SIP Ladder, entries live only for the process lifetime and are exported on demand.
- LMPE (ETSI TS 103 698) itself remains **NOT STARTED**; this task is a diagnostics foundation only and does not implement LMPE encoding/decoding (see [lmpe.md](lmpe.md)).
