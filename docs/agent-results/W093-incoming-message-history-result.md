# Agent Result — Task W093: Incoming MESSAGE Handling + Message History

## 1. Branch

`feature/w093-incoming-message-history`

## 2. Branch de pornire

`feature/w092-sip-message-foundation`

## 3. Fișiere modificate / adăugate

### Added
- `src/sip/MessageHistoryEntry.h` — the conversational history row model
- `src/sip/MessageHistoryStore.h` / `.cpp` — the store (append/dedup/status/filters' data source)
- `tests/test_message_history.cpp` — 10 test functions
- `docs/message-history.md`
- `docs/agent-prompts/W093-incoming-message-history.md`
- `docs/agent-results/W093-incoming-message-history-result.md` (this file)

### Modified
- `CMakeLists.txt` — new source/header entries for `MessageHistoryEntry`, `MessageHistoryStore`
- `tests/CMakeLists.txt` — `MessageHistoryStore.cpp` added to `CALL_SOURCES` (needed by any target compiling `SipManager.cpp`), new `MESSAGE_HISTORY_SOURCES` list, new `test_message_history` target
- `src/sip/SipAccount.h` / `.cpp` — `onInstantMessage`/`onInstantMessageStatus` callbacks, `instantMessageReceived`/`instantMessageStatusReceived` signals, `sendMessage()` gained a `correlationId` parameter
- `src/sip/SipManager.h` / `.cpp` — relays the two new account signals into `MessageHistoryStore`; `sendSipMessage()` now appends/updates a history entry alongside its existing (unchanged) `MessagingEventStore`-feeding trace log
- `src/gui/panels/MessagingDiagnosticsPage.h` / `.cpp` — new "Message History" table + filters, below the W092 composer
- `docs/sip-message.md`, `docs/messaging-event-store.md`, `docs/project-status.md` — cross-references and status updates

## 4. Cum sunt primite MESSAGE-urile

Two independent things happen for every incoming SIP MESSAGE, unchanged
relative to each other:

1. **Raw trace capture** (Task W090, untouched): `PjsipTraceModule` taps the
   message at the PJSIP transport layer and feeds
   `SipTraceLogger → MessagingDiagnosticsStore → MessagingEventStore` —
   this is what still populates the SIP Ladder and Messaging Diagnostics
   feed, exactly as before this task.
2. **Dedicated application-level callback** (new, this task):
   `Account::onInstantMessage` (pjsua2, `SipAccount::Impl::Account`)
   extracts From/To/Contact/Content-Type/body/Call-ID, is marshaled to the
   Qt thread, and feeds `MessageHistoryStore::appendInbound()` via
   `SipManager::onAccountInstantMessageReceived()` — this is what populates
   the new Message History table.

## 5. Cum se evită duplicatele

Two mechanisms:
- **Disjoint stores by construction**: the callback never writes to
  `SipTraceLogger`/`MessagingEventStore`, and the trace-capture pipeline
  never writes to `MessageHistoryStore`. Each store has exactly one input
  path for inbound messages, so there is no way for the same message to
  produce two rows in the *same* store.
- **Fingerprint dedup inside `MessageHistoryStore`** as a safety net: a
  SHA-1 fingerprint of `from|to|contentType|body|callId` is remembered for a
  2-second window; a repeated `appendInbound()` call with the same
  fingerprint within that window is silently absorbed (no new row, no
  signal), covering the case where the callback itself might fire more than
  once for the same physical message.

## 6. Ce afișează istoricul

A single flat "Message History" table (Time, Dir, Peer, Content-Type,
Preview, Status), inbound and outbound rows mixed chronologically:
timestamp, direction, peer URI (from for inbound, to for outbound),
Content-Type, a 200-char-capped body preview, and an outbound status
(inbound rows show "received"). Filterable by All/Inbound/Outbound/Failed
and by Content-Type. A "Clear History" button clears this table only
(independent of the diagnostics table's own Clear).

## 7. Ce statusuri outbound există

`Queued` → `Submitted` → `Sent` or `Failed`:
- `Queued`: the entry was created the instant `sendSipMessage()` was called, before any PJSIP call.
- `Submitted`: `SipAccount::sendMessage()` returned success — PJSIP accepted the request for transmission (not yet a response).
- `Sent`: a 2xx final response arrived via `Account::onInstantMessageStatus`.
- `Failed`: either the synchronous send failed, or a non-2xx final response arrived.
- `Unknown`: default value, never actually shown for a real outbound row (always at least `Queued`).

Per the task's requirement, `onInstantMessageStatus` **is** available in
pjsua2 and **is** used — outbound status is a genuine confirmation from a
final SIP response when one arrives, not a permanent guess. If it never
arrives, the entry simply stays at whatever it last was (never silently
reported as delivered).

## 8. Ce NU este încă implementat

- Automatic IMDN `delivered`/`displayed` generation.
- Presence.
- XCAP.
- Real MSRP.
- Per-conversation threading — one flat feed.
- Message History persistence across restarts.
- Retry/offline queuing for failed sends.

## 9. Ce este încă diagnostic-only

- The Messaging Diagnostics feed above the History table remains exactly as
  read-only as before — sourced only from the trace-capture pipeline,
  untouched by this task.
- MSRP: still completely untouched, no session ever opened.
- Message History itself is not purely diagnostic (it reflects real sends),
  but it never triggers any transport action on its own — Clear History
  only clears local state.

## 10. Teste rulate

Built and ran on Windows with MSVC (NMake Makefiles generator) against the
`build` tree, `ENABLE_PJSIP=ON` and `BUILD_TESTS=ON`:

```
cmake . && nmake && ctest --output-on-failure
```

Result: **100% tests passed, 41/41**, 0 failed — all 40 pre-existing tests
(no regressions) plus the 1 new suite:

| Test | Result |
|---|---|
| test_message_history (10 test functions) | Passed |

## 11. Limitări

- The 2-second dedup window is a heuristic; two genuinely different
  messages with identical from/to/content-type/body/Call-ID sent within
  that window would also be merged (considered acceptable — Call-IDs are
  unique per request in practice).
- The `onInstantMessageStatus` correlation id is a plain integer
  round-tripped through `void*` (no heap allocation, no leak risk), but it
  is only meaningful within a single process run.
- Outbound entries don't currently populate `profileId` (only inbound does,
  where the association is unambiguous from which account's callback fired).
- Same underlying transport/parsing limitations inherited from Tasks
  W090–W092 (see their own docs).

## 12. Commituri

1. `c30d1b7` — `feat(messaging): add incoming message callback mapping`
2. `eaa00f4` — `feat(messaging): add message history model`
3. `f591359` — `feat(ui): add message history filters`
4. `df8c50e` — `test(messaging): add incoming history tests`
5. (this commit) — `docs(messaging): document incoming message history`

## 13. git status

Clean after this commit — all listed files committed on
`feature/w093-incoming-message-history`.

## 14. Push status

Pushed to `origin/feature/w093-incoming-message-history`. Not merged into
`main` or any release branch.
