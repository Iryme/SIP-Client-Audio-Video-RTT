# Messaging Diagnostics

**Task W090** — Added in branch `feature/w090-msrp-lmpe-diagnostics`.
**Task W091** — Extended in branch `feature/w091-messaging-event-store`: the
UI/export layer now reads from the transport-independent `MessagingEventStore`
instead of talking to `MessagingDiagnosticsStore` directly. See
[messaging-event-store.md](messaging-event-store.md) for the new model.

## Overview

This task adds a read-only diagnostics foundation for SIP instant messaging: SIP
MESSAGE requests/responses, CPIM-wrapped bodies, IMDN disposition notifications,
RFC 3994 is-composing indications, and MSRP-related SDP attributes (`m=message`,
`a=path`, `a=accept-types`, `a=setup`, `a=connection`). It captures, parses,
logs, and displays this traffic — it does **not** implement real MSRP sessions,
does not send/receive instant messages itself, and MSRP is never started
implicitly. Everything here is strictly diagnostic, mirroring the existing
[SIP Ladder / SIP Diagnostics](sip-diagnostics.md) feature.

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
| `bodyPreview` | Safe, single-line, whitespace-collapsed, length-capped (160 chars) preview for the UI/export table — a display convenience, not a security redaction |
| `rawSip` | Full raw SIP text; Authorization/Proxy-Authorization values are **already redacted** by `SipTraceLogger` before the entry is built |
| `contentKind` | `MessagingContentKind` detected from `Content-Type` |
| `cpim`, `imdn`, `isComposing`, `sdpMsrp` | Structured info, present only when detected (see below) |

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

### `MessagingDiagnosticsStore` (`src/sip/MessagingDiagnosticsStore.h/.cpp`)

Singleton, same shape as `SipTraceLogger`:
- `entries()` — read-only ordered list.
- `clear()` — empties the list, emits `cleared`.
- `exportToText()` / `exportToJson()` — include base fields, detected
  `contentKind`, and all structured CPIM/IMDN/is-composing/MSRP sections that
  were detected; `rawSip` is included and is already credential-redacted.
- `isMessagingRelevant()` / `buildEntry()` are `static` so they can be (and
  are) unit tested directly without going through the signal chain.

### `MessagingDiagnosticsPage` (`src/gui/panels/MessagingDiagnosticsPage.h/.cpp`)

"Messaging" nav-rail page (own icon, between "SIP Ladder" and "History" —
does not crowd call control). Read-only `QTableWidget` feed sourced from
`MessagingEventStore` (Task W091; see [messaging-event-store.md](messaging-event-store.md))
with columns Time / Direction / Transport / From / To / Call-ID /
Content-Type / Preview / Parse, filters (Call-ID substring, payload type,
direction), and Clear / Export Text / Export JSON buttons acting on the
event store. Parse warnings are shown inline in the "Parse" cell (with the
full list in the cell's tooltip) — never as a blocking dialog, so a
malformed body never interrupts the live feed or the UI thread. A visible
banner states the feature is read-only and never starts an MSRP session.
Row double-click resolves the event's structured CPIM/IMDN/is-composing/
SDP-MSRP detail from `MessagingDiagnosticsStore` (by id, not re-parsed) and
opens `MessagingMessageDetailsDialog` (`src/gui/MessagingMessageDetailsDialog.h/.cpp`),
mirroring `SipMessageDetailsDialog`.

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
| `tests/test_messaging_diagnostics_store.cpp` | Relevance filtering, CPIM→IMDN nesting, is-composing, INVITE+MSRP-SDP relevance, clear/export |
| `tests/test_messaging_event_store.cpp` | See [messaging-event-store.md](messaging-event-store.md) |

Run (see [build-windows.md](build-windows.md) for full environment setup):

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_cpim_parser test_imdn_parser test_is_composing_parser test_sdp_msrp_diagnostics_parser test_messaging_diagnostics_store test_messaging_event_store
ctest --test-dir build_tests -R "test_cpim_parser|test_imdn_parser|test_is_composing_parser|test_sdp_msrp_diagnostics_parser|test_messaging_diagnostics_store|test_messaging_event_store" --output-on-failure
```

All 6 suites (W090 + W091) pass locally.

## Known Limitations

- No real MSRP session is ever started — MSRP diagnostics are detection-only, by design (see [msrp-diagnostics.md](msrp-diagnostics.md)).
- No SIP MESSAGE is sent by the client itself; this task only observes traffic already captured for the SIP Ladder. Sending/composing messages is a future task.
- IMDN `original-recipient` / `final-recipient` are not part of the core RFC 5438 `<imdn>` schema (they originate from RFC 8098 email-style disposition notifications); this parser extracts them opportunistically if present but they will usually be absent in a strict RFC 5438 IMDN body.
- CPIM nesting is one level deep: a CPIM body whose inner `Content-Type` is itself `message/cpim` is not recursively unwrapped.
- `bodyPreview` is a UI truncation convenience, not a redaction pass; the only redaction applied to message content is the existing `SipTraceLogger::redactCredentials()` (Authorization/Proxy-Authorization headers), which runs before a trace ever reaches this feature.
- No persistence — like the SIP Ladder, entries live only for the process lifetime and are exported on demand.
- LMPE (ETSI TS 103 698) itself remains **NOT STARTED**; this task is a diagnostics foundation only and does not implement LMPE encoding/decoding (see [lmpe.md](lmpe.md)).
