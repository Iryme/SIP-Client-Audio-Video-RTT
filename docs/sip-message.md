# SIP MESSAGE Foundation

**Task W092** — Added in branch `feature/w092-sip-message-foundation`, built on
top of [Messaging Diagnostics (W090)](messaging-diagnostics.md) and the
[Messaging Event Store (W091)](messaging-event-store.md).
**Task W093** — [Message History](message-history.md) (branch
`feature/w093-incoming-message-history`) adds a dedicated pjsua2
`onInstantMessage`/`onInstantMessageStatus` callback pair on top of the send
path described here: outbound status now confirms `Sent`/`Failed` via
`onInstantMessageStatus` (previously "submitted only", see that task's
Limitations at the time), and inbound messages get a proper conversational
history feed in addition to the diagnostics table.
**Task W095** — Extended in branch `feature/w095-deflate-rcs-diagnostics`:
an inbound SIP MESSAGE carrying `Content-Encoding: deflate` (observed in
real Linphone iOS interop testing) is now decoded before the
`MessagingDiagnosticsStore` pipeline attempts to parse its body — see
[content-encoding-diagnostics.md](content-encoding-diagnostics.md). Purely a
diagnostics-pipeline change: the send path described in this document never
emits `Content-Encoding` and is unaffected.
**Task W096** — [IMDN Foundation](imdn.md) (branch
`feature/w096-imdn-foundation`) adds `ComposedSipMessage::messageId`
(structural copy of the `Message-ID` header already generated when
`requestImdn` is set) and a new `SipMessageComposer::composeImdnReport()`
builder for outbound `message/imdn+xml` reports, reusing this document's
send path (`SipManager::sendSipMessage()`) unchanged. `SipAccount`'s
`onInstantMessage` callback now also extracts the `Message-ID` and
`Disposition-Notification` header values for the live receive path.

## Overview

This task adds basic send/receive support for SIP MESSAGE (RFC 3428) to the
Windows client: composing a message (plain text, HTML, or CPIM-wrapped),
optionally requesting an IMDN, sending it via PJSIP, and seeing both outbound
and inbound SIP MESSAGE traffic flow through the existing Messaging
Diagnostics feed and event store. **MSRP remains completely disabled** — this
task only implements the out-of-dialog SIP MESSAGE method; no MSRP session is
ever opened, negotiated, or accepted.

No parsing logic from W090 is duplicated: composing builds a synthetic raw
SIP text, which is handed to `SipTraceLogger` exactly like every other trace
in this app, and the existing (unmodified) `MessagingDiagnosticsStore` /
`MessagingEventStore` pipeline parses and maps it, same as for a captured
wire message.

## Architecture

```
MessagingDiagnosticsPage (compose UI)
      │  user clicks Send
      ▼
SipMessageComposer::compose()        — pure Qt, no PJSIP/network dependency
      │  validates destination (SipUriNormalizer) + body,
      │  wraps CPIM (CpimBuilder) if selected, adds IMDN-request headers,
      │  builds a synthetic rawSip text
      ▼
   ComposedSipMessage
      │
      ▼
SipManager::sendSipMessage()
      │  1) logs a synthetic outbound SipMessageTrace (same pattern as
      │     makeCall()'s INVITE trace) — always, even if the send below fails
      │  2) delegates the actual wire send to SipAccount::sendMessage()
      ▼
SipAccount::sendMessage()            — PJSIP only
      │  transient, non-subscribing pjsua2 Buddy;
      │  Buddy::sendInstantMessage() — fire-and-forget, non-blocking
      ▼
   (SIP MESSAGE on the wire)

Inbound SIP MESSAGE requests need no new code: PjsipTraceModule already taps
every raw SIP request/response at the transport layer (Task W090), so an
incoming MESSAGE is captured automatically and flows through the same
MessagingDiagnosticsStore -> MessagingEventStore pipeline as outbound
messages. pjsua's default UA behavior auto-responds 200 OK to an incoming
MESSAGE when no application-level IM callback is registered — this task does
not currently override that (see "What is NOT implemented").
```

## `SipMessageComposer` (`src/sip/SipMessageComposer.h/.cpp`)

Pure Qt, no PJSIP or network dependency — fully unit-testable. Builds a
`ComposedSipMessage` from `SipMessageComposer::Options`:

| Option | Purpose |
|---|---|
| `toUri` | User-entered recipient; validated via `SipUriNormalizer::normalize()` (same helper used elsewhere in the app — no new URI-parsing logic) |
| `fromUri` | Sender URI, taken from the active SIP profile |
| `contentType` | `MessagingContentKind::PlainText`, `Html`, or `Cpim` only — anything else is rejected |
| `body` | User-authored plain-text message |
| `cpimEnabled` | Gates `contentType == Cpim`; mirrors `AppSettings::enableCpim()` |
| `requestImdn` | Adds IMDN-request headers only (see below) — does not generate an IMDN document |

Validation (in order): unsupported content type → invalid destination
(empty/malformed URI) → empty/whitespace-only body → CPIM selected while
`cpimEnabled` is false. Any failure returns `ComposedSipMessage::valid ==
false` with a human-readable `error` and no other side effects.

On success, `compose()`:
- Picks the outer `Content-Type` (`text/plain; charset=utf-8`,
  `text/html; charset=utf-8`, or `message/cpim`).
- For CPIM, wraps the plain-text body via `CpimBuilder::build()` (inner
  `Content-Type` is always `text/plain; charset=utf-8` — the compose UI's
  body editor is plain text).
- For an IMDN request, appends two extra headers: `Message-ID` (a fresh
  UUID) and `Disposition-Notification: positive-delivery, positive-display`
  — both to `extraHeaders` (used for the real wire send) and reflected in
  `rawSip` (used for the diagnostic trace). No IMDN document is generated;
  requesting one only asks the remote UA to send one back later — retry/
  offline handling for that response is explicitly out of scope for W092.
- Builds a synthetic `rawSip` (request line, headers, blank line, body) in
  the same style `SipRawMessageParser` produces for a captured wire message,
  so `MessagingDiagnosticsStore::buildEntry()` can extract and parse the body
  unmodified.
- All string handling is `QString`/`toUtf8()` end to end — UTF-8 body text
  (including large bodies, tested at 5 KB) passes through unmodified.

## `CpimBuilder` (`src/sip/CpimBuilder.h/.cpp`)

The serialization counterpart of `CpimParser` (parse-only, Task W090).
Produces the same header-block + blank-line + body shape `CpimParser::parse()`
expects:

```
From: <from>
To: <to>
DateTime: <ISO 8601>
Content-Type: <inner content type>

<body>
```

A message built by `CpimBuilder::build()` round-trips through the existing,
unmodified `CpimParser::parse()` (verified by test).

## Sending (`SipAccount::sendMessage`, `SipManager::sendSipMessage`)

`SipManager::sendSipMessage(const ComposedSipMessage &msg, QString &error)`:
1. Rejects an invalid `msg` outright (defensive; the UI already calls
   `SipMessageComposer::compose()` first and only proceeds on success).
2. Always logs a synthetic outbound `SipMessageTrace` via
   `SipTraceLogger::instance().logMessage()` — the same "per-action summary"
   pattern `SipManager::makeCall()` already uses for outbound INVITEs — so
   the attempt is visible in the SIP Ladder / Messaging Diagnostics even if
   the underlying PJSIP send fails below.
3. Requires an active `SipAccount` (i.e. a profile has been registered/
   activated); otherwise returns `false` with an error and stops.
4. Delegates to `SipAccount::sendMessage()`.

`SipAccount::sendMessage()` (PJSIP only): pjsua2's `Account` class has no
`sendInstantMessage()` of its own — only `Buddy` does. This method creates a
**transient, non-subscribing** `pj::Buddy` (`BuddyConfig::subscribe = false`,
`subscribe_dlg_event = false` — no presence or dialog-event SUBSCRIBE is ever
started; the Buddy exists only to route and authenticate one MESSAGE
request) bound to the active account, then calls
`Buddy::sendInstantMessage()` with the composed `Content-Type`, body, and any
extra headers (`pj::SipHeader`/`SipTxOption`, the same pattern already used
for INVITE custom headers in `SipCall.cpp`). The call is fire-and-forget:
pjsua2 submits the request to the transaction layer and returns immediately
— it does not block the Qt/GUI thread waiting for a response. In stub
(non-PJSIP) builds, `sendMessage()` always returns `false` with an
explanatory error; the synthetic trace from step 2 above is still logged so
composing/attempting a send remains visible in diagnostics.

## Receiving

At the time this task (W092) shipped, no inbound-handling code was added:
`PjsipTraceModule` (Task W090) already taps every raw SIP request at the
PJSIP transport layer, method-agnostic, so an incoming SIP MESSAGE was
captured exactly like any other SIP message and flowed through the
unchanged `MessagingDiagnosticsStore` → `MessagingEventStore` pipeline
automatically, with no dedicated "new message" application-level hook.

**Task W093** added that dedicated hook: `Account::onInstantMessage` /
`Account::onInstantMessageStatus` on the same `pj::Account` subclass, feeding
a new, separate `MessageHistoryStore` (never `MessagingEventStore` — see
[message-history.md](message-history.md) for why that separation is what
avoids duplicate rows between the two pipelines). `MessagingEventStore`
itself is untouched by W093 and still only ever sees inbound messages via
the original raw-trace tap.

## CPIM generation

See `CpimBuilder` above. Selecting `message/cpim` as the compose Content-Type
requires `AppSettings::enableCpim()` — if it's off, `compose()` fails
validation and nothing is sent. The CPIM wrapper is generated automatically;
the user only ever types the plain-text body, never CPIM headers.

## IMDN request

Checking "Request IMDN" (`AppSettings::requestImdnByDefault` default; also a
per-send checkbox in the compose panel) adds `Message-ID` and
`Disposition-Notification: positive-delivery, positive-display` headers to
the outbound request. This is a **request for a future IMDN response only**
— this task does not generate `delivered`/`displayed` IMDN documents
automatically (that already exists as parse-only diagnostics from W090 for
any IMDN the *remote* side sends back), and does not implement retry or
offline queuing for IMDN responses.

## Config (`src/core/AppSettings.h`)

Conservative defaults — everything is off until explicitly enabled:

| Setting | Key | Default |
|---|---|---|
| `enableSipMessage()` / `setEnableSipMessage()` | `messaging/enableSipMessage` | `false` |
| `enableCpim()` / `setEnableCpim()` | `messaging/enableCpim` | `false` |
| `requestImdnByDefault()` / `setRequestImdnByDefault()` | `messaging/requestImdnByDefault` | `false` |

None of these settings affect MSRP, which has no enable switch anywhere in
this app and remains permanently off.

Exposed in two places, both writing the same keys:
- **Settings → Text / Accessibility → Messaging Diagnostics**: three
  checkboxes alongside the existing max-events-retained field; persisted on
  Save, reset to `false` by Reset.
- **Messaging Diagnostics page → Send SIP MESSAGE**: the same three
  checkboxes, applied *immediately* on toggle (no Save step) since they
  directly gate the adjacent Send button.

## UI (`src/gui/panels/MessagingDiagnosticsPage.h/.cpp`)

A "Send SIP MESSAGE" group was added to the existing Messaging Diagnostics
page (no new nav-rail entry): recipient field, Content-Type combo
(text/plain, text/html, message/cpim), a plain-text body editor, the three
Enable SIP MESSAGE / Enable CPIM / Request IMDN checkboxes, a Send button,
and an inline status label.

Safety guards:
- Send is disabled whenever Enable SIP MESSAGE is off, the recipient field is
  empty, or the body is empty (`updateSendEnabled()`, re-evaluated on every
  relevant `textChanged`/`toggled` signal).
- Composing failures (invalid/empty destination, empty body, CPIM selected
  without Enable CPIM) are shown in the inline status label — never a
  blocking `QMessageBox` — and stop before anything is logged or sent.
- Send failures (no active account, PJSIP unavailable, pjsua2 exception) are
  likewise shown inline; the attempt is still visible in the diagnostics feed
  because `SipManager::sendSipMessage()` logs the trace before attempting the
  PJSIP call.
- Sending calls straight into `SipManager`/pjsua2's fire-and-forget submit
  path — nothing here blocks the UI thread.

History: at the time this task (W092) shipped, no separate history table
existed — every SIP MESSAGE appeared only as a row in the Messaging
Diagnostics table, with no live "Sent"/"Failed" status. **Task W093** added a
dedicated "Message History" table (with All/Inbound/Outbound/Failed and
Content-Type filters) below this composer, with per-row outbound status that
now upgrades from `Submitted` to `Sent`/`Failed` once PJSIP's
`onInstantMessageStatus` reports a final response — see
[message-history.md](message-history.md).

## Tests (`tests/test_sip_message_foundation.cpp`)

Pure Qt, no PJSIP dependency — mirrors `test_messaging_event_store.cpp`'s
structure.

| Test | Covers |
|---|---|
| `buildsPlainTextMessage` | text/plain composition, Content-Type, rawSip shape |
| `buildsHtmlMessage` | text/html composition |
| `buildsCpimWrapper` | CPIM wrapping when enabled; round-trips through the existing `CpimParser` |
| `cpimRequiresEnableCpim` | CPIM selection rejected when `cpimEnabled == false` |
| `handlesUtf8Body` | non-ASCII UTF-8 body preserved unmodified end to end |
| `handlesLargeBody` | 5 KB body (>4 KB) composed and embedded in rawSip without truncation |
| `requestImdnAddsHeaders` | Message-ID + Disposition-Notification headers added; no IMDN document generated |
| `rejectsEmptyDestination` | empty `toUri` → invalid |
| `rejectsInvalidDestination` | malformed `toUri` (contains a space) → invalid |
| `rejectsEmptyBody` | whitespace-only body → invalid |
| `outboundMessageMapsToMessagingEvent` | composed message's rawSip, logged via `SipTraceLogger` exactly as `SipManager::sendSipMessage()` does, maps into a `MessagingEvent` through the unmodified W090/W091 pipeline |

Run:

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_sip_message_foundation
ctest --test-dir build_tests -R test_sip_message_foundation --output-on-failure
```

All 11 test functions pass locally, alongside the full existing suite
(41/41 total as of Task W093 — see [project-status.md](project-status.md)).

## What remains diagnostic-only / limitations

- MSRP is untouched and permanently disabled — nothing in this task enables,
  negotiates, or opens an MSRP session.
- `SipAccount::sendMessage()` is still fire-and-forget at the point it
  returns (reports success once pjsua2 accepts the request for submission,
  not once a final response is received). **As of Task W093**,
  `Account::onInstantMessageStatus` is wired up and upgrades the Message
  History entry's status to `Sent`/`Failed` once/if a final response
  arrives — see [message-history.md](message-history.md).
- **As of Task W093**, a dedicated `Account::onInstantMessage` callback is
  registered and feeds a proper Message History list — see
  [message-history.md](message-history.md).
- IMDN requests only add headers; no IMDN generation, retry, or offline
  queuing exists (explicitly out of scope per the task).
- No message persistence — like `MessagingEventStore`, everything lives only
  for the process lifetime.
- Single flat history (now the dedicated Message History table, Task W093)
  — no per-conversation threading.
- **Task W095**: `SipMessageComposer` never sends a compressed body — this
  client only ever *decodes* `Content-Encoding: deflate` on receipt, it
  does not compress anything it sends.

## What is NOT implemented by this task

- MSRP sessions of any kind.
- Automatic IMDN `delivered`/`displayed` generation in response to a
  received request (only the *request* headers are added when sending).
- Delivery/read confirmation surfaced back into the compose UI (Task W093
  added final-SIP-response confirmation — `Sent`/`Failed` — in the Message
  History status column; that is not the same as an IMDN delivery/display
  receipt, which still doesn't exist).
- Multiple simultaneous conversations / contact-scoped history.
- Message retry or offline queuing.
