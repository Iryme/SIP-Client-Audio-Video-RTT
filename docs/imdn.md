# IMDN Foundation (RFC 5438)

**Task W100** — [MSRP Foundation](msrp-foundation.md) (branch
`feature/w100-msrp-foundation`) reuses `ImdnParser`/`ImdnGenerator`
unchanged for IMDN reports carried inside an MSRP SEND body
(`message/imdn+xml`) — no IMDN parsing logic is duplicated for the MSRP
transport.

**Task W113F correction** — this doc previously said MSRP-carried IMDN
went through `MsrpPayloadDispatcher`. That class exists and is tested
(`tests/test_msrp_payload_dispatcher.cpp`) but was never actually wired
into the production MSRP receive path — `SipCall`'s MSRP payload signal
went straight to `SipManager` without passing through it. W113F fixed the
real gap this caused (an MSRP-carried IMDN/is-composing/CPIM payload
rendered as a plain chat bubble instead of being classified) by adding the
unwrap-and-classify logic directly in
`SipManager::routeInboundMessagingPayload()`, shared by both the plain SIP
MESSAGE and MSRP inbound paths — see
[messaging-content-type-routing.md](messaging-content-type-routing.md).
**Task W096** — Added in branch `feature/w096-imdn-foundation`, built on top of
[SIP MESSAGE Foundation (W092)](sip-message.md), [Message History
(W093)](message-history.md), and the read-only IMDN *parsing* support
introduced by W090's `ImdnParser`/`ImdnInfo` (see
[Messaging Diagnostics](messaging-diagnostics.md)). **MSRP remains
completely disabled** — this task only adds IMDN generation/correlation over
the existing SIP MESSAGE path; no MSRP session is ever opened.
**Task W111** — [Client Messaging Workspace](client-messaging-workspace.md)
adds "Request Delivered"/"Request Displayed" checkboxes to the new
`ClientMessagingView` composer (setting
`SipMessageComposer::Options::requestImdn`) and a "mark as read" action
calling the existing `SipManager::sendDisplayedImdnForEntry()` — no new
IMDN generation/correlation logic. Separately, this task also closed an
MSRP-side gap this document's correlation didn't cover: MSRP delivery
status (`SipCall::msrpDeliveryStatusChanged`) was previously only logged,
never correlated into `MessageHistoryStore` — now handled by the new
`MessageHistoryStore::correlateMsrpDelivery()`, keyed on MSRP Message-ID,
which is a **separate id space** from the SIP Message-ID
`correlateDelivery()` below matches on; the two are never cross-matched
(see `tests/test_message_history.cpp`
`correlateMsrpDeliveryIgnoresSipMessageId`).

## Overview

Prior to W096 this client could only *parse* an IMDN report it happened to
receive (diagnostics-only, in `MessagingDiagnosticsStore`/`MessagingEventStore`).
It never generated one, and a sent message's delivery/read state was never
tracked past "the SIP MESSAGE request was accepted for transmission"
(`MessageHistoryEntry::OutboundStatus`).

W096 closes that loop:

- **Generation** (`ImdnGenerator`) — builds RFC 5438 `message/imdn+xml`
  bodies for `delivered`/`displayed`/`failed`/`error` reports.
- **Auto Delivered** — when an inbound SIP MESSAGE carries
  `Disposition-Notification: positive-delivery` and a `Message-ID` header,
  a `delivered` report is sent back automatically (default **ON**).
- **Displayed** — never sent automatically by default (default **OFF**);
  either enabled explicitly (`Auto Send Displayed IMDN`) or triggered
  manually via the "Mark as Read" action in Message History.
- **Correlation** — outbound messages that requested an IMDN carry a
  `Message-ID`; when a matching report arrives, the *original* outbound
  `MessageHistoryEntry` is upgraded to `Delivered`/`Displayed`/`Failed`/
  `Error` (`MessageHistoryEntry::DeliveryState`).
- **Parsing** — `ImdnParser`/`ImdnInfo::Disposition` extended with
  `Forbidden`/`Processed` (RFC 5438 §5.1 + observed extension statuses).

Nothing in this task duplicates existing parsing/classification logic: an
inbound `message/imdn+xml` body flowing through the diagnostics pipeline
(`SipTraceLogger` → `MessagingDiagnosticsStore` → `MessagingEventStore`) is
still parsed exactly as before by the unmodified `ImdnParser`; W096 only adds
a second, independent live-processing path in `SipManager` that reacts to
the same PJSIP `onInstantMessage` callback Message History already
consumes (Task W093), and reads the already-parsed `ImdnInfo` when
classifying diagnostics events (see "Diagnostics surfacing" below).

## Pre-implementation check (byte-safety)

Before implementing, the task required verifying whether the live SIP
MESSAGE send/receive path (as opposed to the raw-trace diagnostics parser
fixed in [W095](content-encoding-diagnostics.md)) risks losing body bytes
via a `QString`/`toLatin1()` round-trip.

**Finding: no change was needed.** The live path
(`SipAccount::sendMessage`/`onInstantMessage`) uses pjsua2's high-level
`SendInstantMessageParam`/`OnInstantMessageParam`, whose `content`/`msgBody`
are `std::string` already decoded/encoded as text by PJSIP itself — there is
no raw-bytes-through-`QString` step in this path (unlike
`SipRawMessageParser`'s raw-wire-text parsing, which W095 already made
binary-safe via `SipBodyExtractor`). IMDN report bodies are XML text
(UTF-8), so no binary-safety concern applies to them either.

## Architecture

```
Inbound SIP MESSAGE (pjsua2 onInstantMessage)
      │  SipAccount now also extracts the Message-ID and
      │  Disposition-Notification headers (generic pjsip_msg_find_hdr_by_name
      │  lookup — the same pattern already used for the From header)
      ▼
SipAccount::instantMessageReceived(..., messageId, dispositionNotification)
      ▼
SipManager::onAccountInstantMessageReceived()
      │
      ├─ Content-Type == message/imdn+xml?
      │     │ yes
      │     ▼
      │  ImdnParser::parse(body)              — Task W090, unchanged
      │     │
      │     ├─ MessageHistoryStore::appendInboundImdn(...)   — own history row
      │     └─ MessageHistoryStore::correlateDelivery(info.messageId, state)
      │           — upgrades the matching OUTBOUND entry's deliveryState
      │
      └─ plain message
            │
            ├─ MessageHistoryStore::appendInbound(..., messageId, dispositionNotification)
            │
            ├─ wants delivered && AppSettings::autoSendDeliveredImdn() (default ON)
            │     → SipManager::sendImdnReport(..., Delivered)
            │
            └─ wants displayed && AppSettings::autoSendDisplayedImdn() (default OFF)
                  → SipManager::sendImdnReport(..., Displayed)

Manual "Mark as Read" (Message History UI)
      ▼
SipManager::sendDisplayedImdnForEntry(entryId)
      │  looks up the entry, checks displayNotificationRequested/!displayedImdnSent
      ▼
SipManager::sendImdnReport(toUri, originalMessageId, Displayed, entryId)
      │
      ▼
SipMessageComposer::composeImdnReport()      — pure builder, message/imdn+xml body
      │  built via ImdnGenerator::generate()
      ▼
SipManager::sendSipMessage()                  — same send path as W092, unchanged
      │  logs synthetic outbound trace (feeds diagnostics too, as an IMDN event)
      ▼
SipAccount::sendMessage()                     — PJSIP, unchanged
```

## `ImdnGenerator`

`src/sip/ImdnGenerator.h/.cpp` — pure Qt/text, no PJSIP dependency, fully
unit-testable.

```cpp
static QString generate(const QString &messageId,
                        ImdnInfo::Disposition disposition,
                        const QString &originalRecipient = QString(),
                        const QString &finalRecipient = QString(),
                        const QDateTime &datetime = QDateTime());
```

- Returns an empty string (caller must treat as failure) when `messageId` is
  empty or `disposition == None` — an IMDN report is meaningless without
  either.
- `delivered`/`failed`/`forbidden` are wrapped in `<delivery-notification>`;
  `displayed`/`error`/`processed` in `<display-notification>` (RFC 5438
  §5.1). This client only ever *generates* `delivered`/`displayed`/`failed`/
  `error` — `forbidden`/`processed` are inbound-only statuses it can parse
  but never emits itself.
- Element names (`<message-id>`, `<original-recipient>`,
  `<final-recipient>`) intentionally match what the existing `ImdnParser`
  already recognizes (not the RFC 5438 schema's `-uri`-suffixed variants),
  so a generated report round-trips through the unmodified parser.
- All free-text values (`messageId`, recipients) are XML-escaped.

## Auto Delivered / Displayed policy

Two independent `AppSettings` toggles (`src/core/AppSettings.h`):

| Setting | Key | Default | Rationale |
|---|---|---|---|
| Auto Send Delivered IMDN | `messaging/autoSendDeliveredImdn` | **ON** | A transport-level acknowledgement; does not disclose whether/when the message was read. |
| Auto Send Displayed IMDN | `messaging/autoSendDisplayedImdn` | **OFF** | Discloses that the user has read the message — a privacy-sensitive default requiring explicit opt-in, matching the task's exact requirement. |

Both are exposed as checkboxes in the Messaging Diagnostics page's "Send SIP
MESSAGE" group, next to the existing `Enable SIP MESSAGE`/`Enable
CPIM`/`Request IMDN` toggles.

When `Auto Send Displayed IMDN` is OFF, the user marks a message as read
manually: select the inbound row in the Message History table and click
**Mark as Read** (enabled only when the sender requested a display
notification, the entry has a `Message-ID`, and a Displayed report has not
already been sent for it).

## Message-ID correlation

RFC 5438 correlates a disposition report to its original message via the
`Message-ID` header (not the SIP `Call-ID`).

- **Outbound**: `SipMessageComposer::compose()` already generated a
  `Message-ID` header whenever `requestImdn` is set (Task W092); W096 adds
  `ComposedSipMessage::messageId`, a structural copy of that same value, so
  `MessageHistoryStore::appendOutbound()` can capture it on the entry
  without re-parsing `extraHeaders`.
- **Inbound**: `SipAccount::onInstantMessage` now also extracts the
  `Message-ID` and `Disposition-Notification` header values directly from
  the parsed `pjsip_msg` (via `pjsip_msg_find_hdr_by_name`, the same
  low-level pattern already used for `Call-ID`/`From`), and passes them
  through `instantMessageReceived`.
- **Correlation**: `MessageHistoryStore::correlateDelivery(messageId, state)`
  finds the most recent **outbound**, non-IMDN-report entry whose
  `messageId` matches and upgrades its `deliveryState`. It is a no-op
  (idempotent, no signal emitted) when no match is found — e.g. the
  Message-ID was never one this client generated, or the entry was evicted.
- **History fields added**: `MessageHistoryEntry::messageId`,
  `correlatedMessageId`, `isImdnReport`, `deliveryState`, plus (inbound-only)
  `deliveryNotificationRequested`, `displayNotificationRequested`,
  `deliveredImdnSent`, `displayedImdnSent`.
- **Lifecycle example**: `Queued` → `Submitted` (`outboundStatus`,
  unchanged from W093) while `deliveryState` stays `None`; once the peer's
  `delivered` report arrives, `deliveryState` becomes `Delivered`; a later
  `displayed` report upgrades it to `Displayed`. A duplicate/retransmitted
  IMDN report is deduped by `MessageHistoryStore`'s existing inbound
  fingerprint mechanism (same window/logic as W093's plain-message dedup),
  so re-applying the same disposition is harmless.

## IMDN parser extension

`ImdnInfo::Disposition` gained `Forbidden` and `Processed` (RFC 5438 §5.1
delivery-notification statuses include `forbidden`; some peers — observed
in RCS interop — emit an extension `processed` display-notification
status). `ImdnParser` recognizes both; this client never generates them.

## UI: Message History status

`MessagingDiagnosticsPage`'s Message History table's Status column now
renders icon-decorated text (matching the task's exact example):

| State | Rendering |
|---|---|
| Outbound, no report yet | `… Queued` / `✓ Submitted` / `✓ Sent` |
| Outbound, Delivered report received | `✓✓ Delivered` |
| Outbound, Displayed report received | `👁 Displayed` |
| Outbound, Failed/Error report received | `⚠ Failed` / `⚠ Error` |
| Outbound send itself failed | `⚠ Failed` |
| Inbound, plain message | `received` (or `received (unread)` while a display notification is pending) |
| Inbound, IMDN report | `IMDN report` |

All updates happen via the existing `entryAppended`/`entryUpdated` signals
(row lookup by id, no full-table rebuild) — the UI thread is never blocked
waiting on a send.

## Diagnostics surfacing (`MessagingEvent`)

The read-only diagnostics pipeline (`MessagingEventStore`, fed from
`MessagingDiagnosticsStore`'s raw-trace capture — completely independent of
`MessageHistoryStore`, see [Message History](message-history.md) for why)
gained four additive fields, populated in `mapFromTraceEntry()` purely from
the already-parsed `MessagingTraceEntry::imdn` (no new parsing):

- `generatedImdn` (bool) — this event's body is an outbound IMDN report.
- `receivedImdn` (bool) — this event's body is an inbound IMDN report.
- `correlatedMessageId` (string) — `ImdnInfo::messageId`, the original
  message this report is about.
- `deliveryState` (string) — `ImdnInfo::dispositionToString(...)`.

These are exported by both `MessagingEventStore::exportToJson()` and
[`InteropTraceExporter`](windows-trace-json-export.md) — see that doc for
the JSON schema addition.

## What remains diagnostic-only / out of scope

- **MSRP** — never activated by this task, as required.
- **Presence / XCAP** — not implemented, as required.
- IMDN `Adler`/checksum-equivalent integrity is not applicable (XML text,
  no compression); this task does not touch [W095's deflate
  decoding](content-encoding-diagnostics.md) at all.
- No retry/backoff for a failed IMDN send — `sendImdnReport()` reports
  success/failure once; a failed auto-Delivered/Displayed send is logged
  but not retried.
- No UI badge/counter for "unread messages" beyond the per-row
  `received (unread)` status text.

## Tests

`tests/test_imdn_parser.cpp` — `ImdnGenerator` generation + round-trip
through `ImdnParser` (delivered/displayed/failed/error), empty-messageId/
None-disposition rejection, UTF-8 + XML-special-character escaping, plus
the pre-existing parser coverage extended with `forbidden`/`processed`.

`tests/test_sip_message_foundation.cpp` — `SipMessageComposer::composeImdnReport()`
(delivered/displayed bodies, rejection when `originalMessageId` is empty),
and a diagnostics-mapping test verifying a generated IMDN report's
`generatedImdn`/`correlatedMessageId`/`deliveryState` fields once it flows
through the unmodified W090/W091 pipeline.

`tests/test_message_history.cpp` — Message-ID capture (inbound header,
outbound when IMDN requested), `correlateDelivery` (upgrade,
Delivered→Displayed progression, unknown-Message-ID no-op), duplicate
inbound IMDN report dedup, `markImdnSent` flag independence
(Delivered/Displayed tracked separately), `appendInboundImdn` field mapping
— plus full regression coverage of the pre-existing W093 History tests
(dedup, filtering, preview limit, UTF-8 body).

`tests/test_messaging_diagnostics_store.cpp`'s pre-existing
`cpimWrappedImdnIsParsed()` test is an unmodified CPIM+IMDN regression
check — the CPIM-unwrap-then-IMDN-parse code path in
`MessagingDiagnosticsStore::buildEntry()` is untouched by W096.

Run: `cmake --build build --target all` (with `ENABLE_PJSIP=ON`,
`BUILD_TESTS=ON`) then `ctest --output-on-failure` from `build/`.
