# SIP MESSAGE Foundation

**Task W092** — Added in branch `feature/w092-sip-message-foundation`, built on
top of [Messaging Diagnostics (W090)](messaging-diagnostics.md) and the
[Messaging Event Store (W091)](messaging-event-store.md).

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

No new inbound-handling code was added. `PjsipTraceModule` (Task W090)
already taps every raw SIP request at the PJSIP transport layer,
method-agnostic, so an incoming SIP MESSAGE is captured exactly like any
other SIP message and flows through the unchanged
`MessagingDiagnosticsStore` → `MessagingEventStore` pipeline automatically.
pjsua's default UA behavior sends `200 OK` to an incoming MESSAGE when no
application-level `Account::onInstantMessage` callback is registered — this
task does not register one, so delivery still succeeds at the SIP level, but
the app has no dedicated "new message" notification hook yet (see below).

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

History: no separate history table was added. Every SIP MESSAGE (outbound
attempts and inbound captures alike) already appears as a row in the
existing Messaging Diagnostics table above the composer — timestamp,
direction, from/to, Content-Type, and body preview are all already columns
there (see [messaging-diagnostics.md](messaging-diagnostics.md)). This keeps
the feed as the single source of truth instead of a second, potentially
inconsistent list. The one piece that table does *not* show is a live
"Sent"/"Failed" submit status for the message just composed — that's shown
transiently in the compose panel's status label instead (see "What remains
diagnostic-only / limitations" below for why final delivery confirmation
isn't wired up).

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
(40/40 total — see [project-status.md](project-status.md)).

## What remains diagnostic-only / limitations

- MSRP is untouched and permanently disabled — nothing in this task enables,
  negotiates, or opens an MSRP session.
- `SipAccount::sendMessage()` is fire-and-forget: it reports success once
  pjsua2 accepts the request for submission, not once a `200 OK` (or any
  other final response) is received. `Account::onInstantMessageStatus` (the
  pjsua2 callback that reports the final SIP transaction result) is not
  wired up, so the compose panel's "Sent" status reflects successful
  *submission*, not confirmed delivery.
- No `Account::onInstantMessage` callback is registered, so there is no
  dedicated "new message received" application-level event/notification —
  inbound messages are only visible by way of the existing diagnostics feed
  (which already updates live), not a toast/badge/unread-count.
- IMDN requests only add headers; no IMDN generation, retry, or offline
  queuing exists (explicitly out of scope per the task).
- No message persistence — like `MessagingEventStore`, everything lives only
  for the process lifetime.
- Single flat history (the existing diagnostics table) — no per-conversation
  threading.

## What is NOT implemented by this task

- MSRP sessions of any kind.
- Automatic IMDN `delivered`/`displayed` generation in response to a
  received request (only the *request* headers are added when sending).
- Delivery/read confirmation surfaced back into the compose UI.
- Multiple simultaneous conversations / contact-scoped history.
- Message retry or offline queuing.
