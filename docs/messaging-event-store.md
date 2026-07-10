# Messaging Event Store

**Task W091** — Added in branch `feature/w091-messaging-event-store`, built on
top of [Task W090's Messaging Diagnostics foundation](messaging-diagnostics.md).
**Task W092** — [SIP MESSAGE Foundation](sip-message.md) (branch
`feature/w092-sip-message-foundation`) sends outbound messages by logging a
synthetic trace into the same `SipTraceLogger → MessagingDiagnosticsStore`
pipeline this store already listens to — no changes were needed here.
Outbound and inbound SIP MESSAGE events both map into `MessagingEvent`
exactly as before.
**Task W093** — [Message History](message-history.md) (branch
`feature/w093-incoming-message-history`) adds a dedicated pjsua2
incoming-message callback and a separate `MessageHistoryStore` for a simple
conversational history view. This store is explicitly **not** touched by
that callback — `MessagingEventStore` keeps being fed exclusively by the
trace-capture pipeline, which is what keeps the same inbound message from
being logged twice into two different places within this store. See
[message-history.md](message-history.md#why-a-separate-store-from-messagingeventstore)
for the full reasoning.
**Task W096** — [IMDN Foundation](imdn.md) (branch
`feature/w096-imdn-foundation`) adds `generatedImdn`/`receivedImdn`/
`correlatedMessageId`/`deliveryState` to `MessagingEvent`, populated in
`mapFromTraceEntry()` purely from the already-parsed
`MessagingTraceEntry::imdn` (`ImdnInfo`, Task W090's `ImdnParser`) — no new
parsing added here. This is a diagnostics-only surfacing of IMDN reports
seen on the wire; it is independent of the live delivery/read tracking that
W096 adds to `MessageHistoryStore` (see [imdn.md](imdn.md)).
**Task W097** — [Active is-composing](is-composing.md) (branch
`feature/w097-is-composing`) similarly adds `generatedIsComposing`/
`receivedIsComposing`/`typingState`/`typingRefresh`/`typingTimeout` to
`MessagingEvent`, populated purely from the already-parsed
`MessagingTraceEntry::isComposing` (Task W090's `IsComposingParser`) — no
new parsing here either.

## Overview

This task introduces `MessagingEvent`, a unified, **transport-independent**
model for messaging activity, and `MessagingEventStore`, the store that
maintains it. The goal: the UI and export layer should not need to know
whether a diagnostic row came from a SIP MESSAGE trace or (in a future task)
a real MSRP chunk — they only depend on `MessagingEvent`.

This task does **not** add any new parsing. `MessagingEventStore` maps
`MessagingTraceEntry` rows already built by `MessagingDiagnosticsStore` (Task
W090) into `MessagingEvent` rows — the CPIM/IMDN/is-composing/SDP-MSRP
parsers themselves are untouched and are not duplicated in the UI or in this
store.

Same permanent constraints as W090 still apply: no real MSRP session is ever
started, no SIP MESSAGE is sent by this code, everything is read-only/
diagnostic, and nothing here changes what is offered/accepted on the wire.

## Architecture

```
MessagingDiagnosticsStore::entryLogged(MessagingTraceEntry)   (Task W090 — unchanged)
          │
          ▼
MessagingEventStore::onDiagnosticsEntryLogged()
          │  mapFromTraceEntry() — pure mapping, no re-parsing
          ▼
      MessagingEvent
          │  append() — mutex-protected, evicts oldest once
          │  maxEventsRetained() is exceeded
          ▼
   MessagingEventStore::m_events (bounded QList<MessagingEvent>)
          │  emits eventAppended(MessagingEvent) / cleared()
          ▼
MessagingDiagnosticsPage   (reads snapshot()/eventAppended, not
                             MessagingDiagnosticsStore directly)
```

`MessagingDiagnosticsStore::cleared()` is still the single source of truth
for "Clear": the page's Clear button calls
`MessagingDiagnosticsStore::instance().clear()`, which cascades to
`MessagingEventStore` via its `onDiagnosticsCleared()` slot. This keeps the
two stores' entry ordering aligned (see "id alignment" below) without a
second, independent clear path.

## `MessagingEvent` (`src/sip/MessagingEvent.h`)

Header-only struct (mirrors `SipMessageTrace`'s style):

| Field | Type | Purpose |
|---|---|---|
| `id` | `qint64` | Internal unique id, assigned sequentially by `MessagingEventStore` starting at 1 |
| `timestamp` | `QDateTime` | Event time |
| `direction` | `Direction` (`Inbound`/`Outbound`/`Unknown`) | |
| `transport` | `Transport` (`SipMessage`/`Msrp`/`Unknown`) | `SipMessage` for SIP MESSAGE requests/responses; `Msrp` for SDP offers/answers diagnosing an (unopened) MSRP negotiation |
| `payloadType` | `PayloadType` (`Plain`/`Html`/`Cpim`/`Imdn`/`IsComposing`/`Sdp`/`Unknown`) | Mapped 1:1 from `MessagingContentKind` |
| `from`, `to`, `callId`, `cseq`, `contentType` | `QString` | Copied from the underlying trace |
| `bodyPreview` | `QString` | Same safe, truncated preview built by `MessagingDiagnosticsStore` |
| `rawSipRedacted` | `QString` | Full raw SIP text, already credential-redacted upstream by `SipTraceLogger` |
| `parseStatus` | `ParseStatus` (`Ok`/`Partial`/`Error`) | See "Parse warnings" below |
| `parseWarnings` | `QStringList` | Human-readable notes about parsing gaps for this event |

All enum `*ToString()` helpers are `static` methods on the struct, matching
the pattern used by `SipMessageTrace`/`MessagingTraceEntry`'s sibling info
structs.

## `MessagingEventStore` (`src/sip/MessagingEventStore.h/.cpp`)

Singleton (`instance()`), same shape as `MessagingDiagnosticsStore`, plus:

- `append(const MessagingEvent &)` — appends and evicts the oldest event(s)
  once `maxEventsRetained()` is exceeded.
- `clear()` — empties the list, resets id numbering to 1, emits `cleared()`.
- `count()` — current event count.
- `snapshot()` — a thread-safe **copy** of the current event list.
- `exportToJson()` / `exportToText()` — same shape/fields as
  `MessagingDiagnosticsStore`'s exporters, plus `parseStatus`/`parseWarnings`.
- `maxEventsRetained()` / `setMaxEventsRetained(int)` — runtime-adjustable
  retention limit; the constructor seeds it from
  `AppSettings::loadMaxMessagingEventsRetained()` (default `1000`).
- `mapFromTraceEntry(const MessagingTraceEntry &, qint64 id)` — `static`,
  exposed for unit testing without a live `MessagingDiagnosticsStore` signal
  chain.

### Thread safety

`m_events` is protected by a `QMutex`; `append()`, `clear()`, `count()`,
`snapshot()`, and `setMaxEventsRetained()` all take the lock. `snapshot()`
returns a copy so callers can iterate without holding the lock. Qt signals
(`eventAppended`, `cleared`) are still expected to be consumed on the
receiver's thread via the normal Qt connection mechanism — in this
application, `MessagingDiagnosticsStore::entryLogged` is already emitted on
the GUI thread (see [messaging-diagnostics.md](messaging-diagnostics.md)),
so no cross-thread marshaling is currently exercised in practice, but the
store itself does not assume single-threaded access.

### Parse warnings

`mapFromTraceEntry()` sets `parseStatus = Partial` and appends a warning
string (instead of silently producing an empty/absent event) when:
- `Content-Type` declares `message/cpim` but no CPIM header block parsed (`CpimInfo::present == false`).
- `Content-Type` declares `message/imdn+xml` but no IMDN disposition parsed.
- `Content-Type` declares `application/im-iscomposing+xml` but no state parsed.
- An SDP `m=message` line was found but no MSRP transport protocol token could be parsed.
- No `Call-ID` is present on the trace.

`parseStatus` stays `Ok` (the default) when none of the above apply.
`ParseStatus::Error` is defined for future stricter validation but is not
currently produced by the mapper — today's parsers are tolerant/best-effort
(see [messaging-diagnostics.md](messaging-diagnostics.md#known-limitations)),
so a hard parse error is not currently distinguishable from "not present."

### id alignment with `MessagingDiagnosticsStore`

`MessagingEventStore` assigns ids sequentially starting at 1, and
`MessagingDiagnosticsStore` never evicts its own `entries()` list
independently. As long as both stores are only ever cleared together (which
is enforced by the single Clear button — see Architecture above), `id - 1`
is a valid index into `MessagingDiagnosticsStore::instance().entries()`, and
`MessagingDiagnosticsPage` uses this to resolve the full structured
CPIM/IMDN/is-composing/SDP-MSRP detail on row double-click without
duplicating any parsing logic in the UI or in `MessagingEventStore` itself.

## Config: max events retained

`AppSettings::loadMaxMessagingEventsRetained()` /
`saveMaxMessagingEventsRetained(int)` (`src/core/AppSettings.h`), key
`messaging/maxEventsRetained`, default `1000`. Exposed in
Settings → Text / Accessibility → "Messaging Diagnostics" as a spin box
(range 50–20000); Save applies it immediately via
`MessagingEventStore::setMaxEventsRetained()` and persists it, Reset restores
the 1000 default. This is a purely in-memory retention limit for diagnostics
rows — it has no effect on SIP/MSRP transport behavior.

## UI integration

`MessagingDiagnosticsPage` (`src/gui/panels/MessagingDiagnosticsPage.h/.cpp`)
was changed to source its table exclusively from `MessagingEventStore`
(`snapshot()` on construction, `eventAppended`/`cleared` signals
thereafter). See [messaging-diagnostics.md](messaging-diagnostics.md) for the
full page description. Parse warnings are rendered inline in a "Parse"
column cell (e.g. `partial (1)`) with the full warning text in the cell's
tooltip — never via a blocking `QMessageBox`, so a malformed body never
stalls the live feed or the UI thread.

## Tests (`tests/test_messaging_event_store.cpp`)

| Test | Covers |
|---|---|
| `appendClearCount` | append/count/snapshot size, clear empties store and emits `cleared` |
| `sipMessageMapsToEvent` | SIP MESSAGE trace → `MessagingEvent` field-by-field mapping, `parseStatus == Ok` |
| `cpimWrappedImdnMapsPayloadType` | CPIM-wrapped body maps to `PayloadType::Cpim` (outer kind; inner IMDN detail remains reachable via `MessagingDiagnosticsStore`) |
| `malformedImdnProducesParseWarning` | Non-XML IMDN body → `parseStatus == Partial` with a non-empty `parseWarnings` |
| `maxEventsRetainedEvictsOldest` | Retention limit evicts oldest-first, newest N retained in order |
| `exportJsonContainsExpectedFields` | JSON export includes `transport`/`payloadType`/`parseStatus`/`parseWarnings` and Call-ID |
| `exportTextContainsExpectedFields` | Text export includes `transport=` and Call-ID |

Run:

```powershell
cmake -S . -B build_tests -G "NMake Makefiles" -DQt6_DIR=<path-to-Qt6-cmake> -DBUILD_TESTS=ON
cmake --build build_tests --target test_messaging_event_store
ctest --test-dir build_tests -R test_messaging_event_store --output-on-failure
```

All 7 test functions pass locally, alongside the full existing suite (39/39
total across the project — see [project-status.md](project-status.md)).

## What is NOT implemented by this task

- No new parsing — CPIM/IMDN/is-composing/SDP-MSRP parsing is exactly what
  Task W090 already provides.
- No real MSRP transport. Sending/composing SIP MESSAGE (plain/HTML/CPIM,
  optional IMDN request) is now implemented — see [sip-message.md](sip-message.md)
  (Task W092) — but this store itself remains an observer; it does not send
  anything. IMDN/is-composing document generation is still not implemented.
- No persistence across app restarts — like `MessagingDiagnosticsStore`,
  `MessagingEventStore`'s in-memory list only lives for the process lifetime.
- No bundle-export integration (`DiagnosticsBundleExporter`) — Messaging
  Diagnostics still has its own standalone Export Text/JSON buttons.
- `MessagingDiagnosticsStore`'s own list is not bounded by
  `maxEventsRetained` — only `MessagingEventStore`'s list is. Detail lookups
  for evicted-from-`MessagingEventStore` events remain possible (via
  `MessagingDiagnosticsStore`) but such rows are simply no longer shown in
  the table because they were evicted from `m_events`.
