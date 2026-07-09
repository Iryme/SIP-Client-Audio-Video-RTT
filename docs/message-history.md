# Message History

**Task W093** — Added in branch `feature/w093-incoming-message-history`, built
on top of [SIP MESSAGE Foundation (W092)](sip-message.md) and
[Messaging Event Store (W091)](messaging-event-store.md).

## Overview

This task adds a dedicated pjsua2 callback for incoming SIP MESSAGE and a
simple conversational Message History list (inbound + outbound, one flat
feed, no threading), independent of the read-only Messaging Diagnostics
feed. No IMDN automation, Presence, XCAP, or real MSRP is implemented here —
none of that is in scope for this task.

## Why a separate store from `MessagingEventStore`

`MessagingEventStore` (Task W091) is fed **exclusively** by the existing raw
SIP trace-capture pipeline (`PjsipTraceModule → SipTraceLogger →
MessagingDiagnosticsStore → MessagingEventStore`, unchanged since Task W090).
That pipeline already captures every inbound SIP MESSAGE automatically,
method-agnostically, at the transport layer — it does not need this task's
new callback to work at all.

This task adds `Account::onInstantMessage`, a dedicated **application-level**
pjsua2 callback that pjsua's IM module invokes specifically for incoming
MESSAGE requests. If this callback's data were *also* logged into
`SipTraceLogger`/`MessagingEventStore`, the same physical wire message would
produce two rows in that store — exactly the "obvious duplicate" the task
warns against.

The resolution: **the two pipelines write to two different stores.**
`MessagingEventStore` keeps being fed only by the trace-capture pipeline (no
changes in this task). The new `MessageHistoryStore` is fed only by this
task's own inputs — the dedicated callback for inbound, and
`SipManager::sendSipMessage()` for outbound — and never reads from or writes
to `MessagingEventStore`/`MessagingDiagnosticsStore`. Both stores observe the
same messages from different angles, but neither one ever gets a duplicate
row from the other's input.

## Architecture

```
Inbound:
  pjsua IM module -> Account::onInstantMessage(OnInstantMessageParam)
        │  (SipAccount::Impl::Account, PJSIP only)
        │  extracts From/To/Contact/Content-Type/body/Call-ID
        ▼
  SipAccount::instantMessageReceived(...)   — QueuedConnection, GUI thread
        ▼
  SipManager::onAccountInstantMessageReceived(...)
        ▼
  MessageHistoryStore::appendInbound(...)   — dedup applied here
        ▼
  entryAppended(MessageHistoryEntry)  ->  MessagingDiagnosticsPage (History table)

  (Unchanged, runs independently: PjsipTraceModule's raw-trace tap still
   captures the same wire message for MessagingDiagnosticsStore /
   MessagingEventStore / the SIP Ladder — Task W090/W091, untouched.)

Outbound:
  SipMessageComposer::compose() -> ComposedSipMessage
        ▼
  SipManager::sendSipMessage()
        │  1) logs synthetic outbound trace (unchanged, Task W092) -> MessagingEventStore
        │  2) MessageHistoryStore::appendOutbound(msg)             -> status Queued
        │  3) SipAccount::sendMessage(..., correlationId=historyId)
        │       -> status Submitted or Failed (synchronous result)
        ▼
  (later, if PJSIP delivers a final response)
  Account::onInstantMessageStatus(OnInstantMessageStatusParam)
        │  correlationId round-tripped through prm.userData (a plain
        │  integer cast through void*, not a heap pointer — nothing to free)
        ▼
  SipAccount::instantMessageStatusReceived(...)  — QueuedConnection
        ▼
  SipManager::onAccountInstantMessageStatusReceived(...)
        ▼
  MessageHistoryStore::updateOutboundStatus(id, Sent | Failed)
        ▼
  entryUpdated(MessageHistoryEntry)  ->  MessagingDiagnosticsPage (row updated in place)
```

## Incoming-message callback (`SipAccount::Impl::Account::onInstantMessage`)

Added in `src/sip/SipAccount.cpp`, alongside the existing `onIncomingCall`/
`onRegState` overrides on the same `pj::Account` subclass. Extracts, per the
task's requirement:

| Field | Source |
|---|---|
| From | `OnInstantMessageParam::fromUri` |
| To | `OnInstantMessageParam::toUri` |
| Contact | `OnInstantMessageParam::contactUri` (empty string if the request had no Contact header) |
| Content-Type | `OnInstantMessageParam::contentType` |
| body | `OnInstantMessageParam::msgBody` |
| timestamp | Set on the Qt thread when the entry is appended (`QDateTime::currentDateTimeUtc()`), same convention `SipTraceLogger::logMessage()` already uses |
| account/profile | `SipAccount::profileId()` — unambiguous, since a signal only ever originates from the account that received the message |
| Call-ID | Extracted from `prm.rdata`'s parsed SIP message (`pjsip_rx_data::msg_info.cid`), used only for the dedup fingerprint and stored on the entry |

Marshaled to the Qt/GUI thread via the same `QPointer` + `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)` pattern used by every other pjsua2 callback in this
file (`onIncomingCall`, `onRegState`) — never touches Qt objects directly
from the PJSIP worker thread.

## Avoiding duplicates

Two independent mechanisms:

1. **Disjoint stores** (see above) — the dedicated callback and the raw
   trace-capture tap never write to the same store, so there is no
   duplication *between* the diagnostics feed and the history feed by
   construction.
2. **Fingerprint dedup within `MessageHistoryStore` itself**
   (`MessageHistoryStore::appendInbound`) — a defensive safety net in case
   the same callback ever fires twice for the same physical message (e.g. a
   spurious re-invocation at the PJSIP layer). A SHA-1 fingerprint of
   `from|to|contentType|body|callId` is remembered for a short window
   (2 seconds); a repeat within that window is silently absorbed (no new
   row, no signal) and the existing entry's id is returned. The fingerprint
   table is opportunistically pruned so it never grows unbounded.

## Message History (`MessageHistoryEntry` / `MessageHistoryStore`)

`src/sip/MessageHistoryEntry.h` — one row:

`id`, `timestamp`, `direction` (`Inbound`/`Outbound`), `peerUri` (from URI for
inbound, to URI for outbound), `contentType`, `bodyPreview` (capped at 200
chars — see "Preview limit and large bodies" below), `outboundStatus`
(`Unknown`/`Queued`/`Submitted`/`Sent`/`Failed` — meaningful for outbound
rows only), `callId`, `contactUri` (inbound only), `profileId`.

`src/sip/MessageHistoryStore.h/.cpp` — singleton, mirrors
`MessagingEventStore`'s shape: `appendInbound()`, `appendOutbound()`,
`updateOutboundStatus()`, `clear()`, `count()`, `snapshot()`,
`maxEntriesRetained()`/`setMaxEntriesRetained()`. Thread-safe via `QMutex`
around the internal list (same pattern as `MessagingEventStore`); signals
(`entryAppended`, `entryUpdated`, `cleared`) are consumed on the GUI thread
via normal Qt connections.

### Outbound status lifecycle

`Queued` (entry created, before the PJSIP call) → `Submitted` (PJSIP
accepted the request for transmission) or `Failed` (composing/PJSIP rejected
it synchronously) → optionally `Sent` (a 2xx final response arrived via
`onInstantMessageStatus`) or `Failed` (a non-2xx final response arrived).
Per the task's requirement 5: PJSIP's `onInstantMessageStatus` callback *is*
available and *is* used here, so outbound status is confirmed via that
callback when it fires; if it never fires (e.g. process shuts down first),
the status simply stays at whatever it last was (`Submitted` or `Failed`) —
it is **never** reported as delivered/read, only submitted or confirmed
sent/failed.

### Preview limit and large bodies

`MessageHistoryStore` never stores the full raw body — only a
whitespace-collapsed, 200-character-capped preview computed once at append
time (`makePreview()`, same technique `MessagingDiagnosticsStore` already
uses for its own preview column). A large inbound/outbound body therefore
never reaches the UI as anything but a short preview string; the full
content, if needed, remains available via the separate Messaging
Diagnostics detail dialog (Task W090/W091, unchanged).

## UI (`src/gui/panels/MessagingDiagnosticsPage.h/.cpp`)

A "Message History" group was added below the "Send SIP MESSAGE" composer
(Task W092) and above the existing diagnostics feed:

- Table columns: Time, Dir, Peer, Content-Type, Preview, Status.
- Filters: a direction/status combo (**All / Inbound / Outbound / Failed**)
  and a Content-Type combo, applied client-side over the in-memory snapshot
  — same pattern as the existing diagnostics table's filters.
- "Clear History" button — clears `MessageHistoryStore` only; it does not
  touch `MessagingEventStore`/`MessagingDiagnosticsStore` (those keep their
  own, unrelated Clear button in the diagnostics toolbar above).
- Status updates (`Submitted` → `Sent`/`Failed`) update the existing row's
  Status cell in place via `entryUpdated`, rather than re-appending a row.
- All updates arrive via queued Qt signal/slot connections
  (`MessageHistoryStore`'s signals, `SipAccount`'s `QueuedConnection`
  callbacks underneath) — nothing in this path runs on or blocks the UI
  thread synchronously with PJSIP.

## Tests (`tests/test_message_history.cpp`)

Pure Qt, no PJSIP dependency. `SipAccount::instantMessageReceived` is a thin
pass-through straight into `MessageHistoryStore::appendInbound()` (see
`SipManager::onAccountInstantMessageReceived`), so exercising
`appendInbound()` directly covers the callback's mapping logic without a
live PJSIP stack.

| Test | Covers |
|---|---|
| `incomingMessageMapsAllFields` | From/To/Contact/Content-Type/body/Call-ID/profileId all mapped onto the entry |
| `dedupSuppressesRepeatedInbound` | three identical `appendInbound()` calls in a row produce only one entry |
| `historyAppendInbound` | count/direction after a single inbound append |
| `historyAppendOutbound` | `appendOutbound()` from a `ComposedSipMessage`, initial status `Queued` |
| `failedOutboundStatus` | `updateOutboundStatus(..., Failed)` updates the entry and emits `entryUpdated` |
| `statusUpgradesFromSubmittedToSent` | `Submitted` → `Sent` transition |
| `filteringByDirectionAndFailed` | direction/failed-status counts over a mixed inbound/outbound/failed set |
| `filteringByContentType` | content-type-based subset counts |
| `previewLimit` | a 500-char body is capped well below its original length |
| `utf8Body` | non-ASCII UTF-8 body preserved unmodified through the preview |

Run:

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_message_history
ctest --test-dir build_tests -R test_message_history --output-on-failure
```

All 10 test functions pass locally, alongside the full existing suite
(41/41 total — see [project-status.md](project-status.md)).

## What is NOT implemented by this task

- Automatic IMDN generation/handling (unchanged from Task W092 — request
  headers only, still no `delivered`/`displayed` document generation).
- Presence or XCAP — not started, out of scope.
- Real MSRP — untouched, permanently disabled.
- Per-conversation threading — one flat inbound+outbound feed.
- Message History persistence across restarts (matches
  `MessagingEventStore`'s existing process-lifetime-only scope).
- Retry/offline queuing for failed sends.

## Limitations

- The dedup window (2 seconds) is a heuristic; two *genuinely different*
  messages with identical from/to/content-type/body/Call-ID sent within that
  window would also be merged — considered acceptable since real Call-IDs
  are unique per request in practice.
- `onInstantMessageStatus`'s correlation id is a plain integer round-tripped
  through `void*` (no heap allocation), so there is no leak risk, but it
  also means the correlation only works within a single process run (never
  persisted, never meaningful across restarts).
- `profileId` on an outbound entry is currently left empty — it isn't filled
  in from `sendSipMessage()` (the sender's own profile is generally the
  single active one already visible elsewhere in the UI); only inbound
  entries populate it, since it is unambiguous there (the account that
  received the callback).
- Same underlying transport/parsing limitations as Tasks W090–W092 (see
  their own docs' Limitations sections).
