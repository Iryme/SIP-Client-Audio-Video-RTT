# Agent Result — Task W113F: Simplify Client Messaging, Hide Protocol XML and Force LMPE Inactive

See [W113F-simplify-client-messaging-and-disable-lmpe.md](../agent-prompts/W113F-simplify-client-messaging-and-disable-lmpe.md)
for the task prompt.

## 1–6. Branch / version

1. **Branch:** `fix/w113f-simplify-client-messaging-and-disable-lmpe`
2. **Branch de pornire:** `fix/w113e-rtt-request-visual-alert` (matched
   the spec exactly; that branch was already complete/pushed)
3. **Versiune veche/nouă:** 1.6.6 → 1.6.7 (spec assumed 1.6.5 → 1.6.6; real
   starting version was already 1.6.6 from W113E — documented substitution
   per the spec's own instruction for this case)
4. **Application version:** 1.6.7
5. **Backend version:** 1.6.7 (single binary, one `PROJECT_VERSION`)
6. **Frontend/UI version:** 1.6.7 (same binary)

## 7. Root cause: XML in history

Two independent inbound paths, each incomplete:

- **Plain SIP MESSAGE** (`SipManager::onAccountInstantMessageReceived`)
  detected `message/imdn+xml`/`application/im-iscomposing+xml` correctly
  but had **no `message/cpim` branch** — a CPIM-wrapped message fell
  through to `appendInbound()` with the raw envelope as its body.
- **MSRP** (`SipCall::msrpPayloadReceived` handler in `SipManager.cpp`)
  classified **nothing** — every MSRP payload went straight to
  `appendInbound()`. A separate, already-tested class,
  `MsrpPayloadDispatcher`, modeled the correct unwrap-and-classify logic
  but was **never called from production code** (confirmed by full-repo
  grep: referenced only in its own `.cpp`, docs, and its own test).

Both root causes are now closed by a single shared function,
`SipManager::routeInboundMessagingPayload()`, called from both paths.

## 8. Content types affected

`message/cpim` (both transports — previously unhandled), `message/imdn+xml`
and `application/im-iscomposing+xml` over MSRP (previously unhandled;
already correct over plain SIP MESSAGE). `application/pidf+xml` was never
affected — it never touched `MessageHistoryStore` (goes straight to
`PresenceStore`). MSRP REPORT/response were never affected — they only
ever update an existing outbound entry's `deliveryState`, never create a
row.

## 9–13. Routing per content type

9. **CPIM**: `routeInboundMessagingPayload()` unwraps first via
   `CpimParser::parse()`; on success, the inner Content-Type/body replace
   the outer ones for every subsequent classification step. On failure,
   `MessageHistoryStore::appendInboundUnsupported()` (placeholder row,
   `isUnsupportedOrMalformed=true`, no raw body stored) and routing stops.
10. **IMDN**: unwrapped or not, detected via
    `MessagingContentKindDetector`, parsed via `ImdnParser`, stored via
    `appendInboundImdn()` (`isImdnReport=true`), delivery state correlated
    onto the original outbound entry via `correlateDelivery()`.
11. **is-composing**: unwrapped or not, parsed via `IsComposingParser`,
    stored via `appendInboundTyping()` (`isTypingNotification=true`),
    consumed by `ConversationModel::remoteTypingState()` for the typing
    indicator.
12. **PIDF**: unchanged — routes to `PresenceStore` via
    `SipAccount::buddyPresenceChanged`, never touches messaging history.
13. **MSRP REPORT**: unchanged — `SipCall::msrpDeliveryStatusChanged` →
    `MessageHistoryStore::correlateMsrpDelivery()`, updates in place, never
    creates a row.

## 14. Unknown content types

Falls through to the existing `appendInbound()` plain-message path
(unchanged behavior for genuinely unrecognized-but-plausibly-text
payloads — the pre-existing conservative binary sniff in
`MsrpPayloadDispatcher` was not ported into the new shared function since
the new function only unwraps CPIM/IMDN/is-composing and otherwise
preserves prior appendInbound()-for-everything-else behavior; this is a
documented limitation, see item 45).

## 15–16. UserMessage / ProtocolEvent model

15. **UserMessage model**: no new enum/type was introduced. The existing
    `MessageHistoryEntry` struct plus its boolean flags
    (`isImdnReport`/`isTypingNotification`/new `isUnsupportedOrMalformed`)
    plus the new `isProtocolEvent()` helper method achieve the practical
    "is this a real message" distinction the spec asked for, with a much
    smaller change surface than a model-wide type rewrite.
16. **ProtocolEvent model**: same struct, same flags — `isProtocolEvent()`
    returns true for exactly the three protocol-event flags. Filtering
    happens at `ConversationModel::userVisibleHistoryFor()`, not by adding
    a second parallel store.

## 17. Client simplification

`ClientMessagingView`'s transport selector, content-type selector, and
delivery-receipt checkboxes moved behind a `QToolButton` disclosure
("Messaging options"), collapsed by default
(`AppSettings::clientMessagingAdvancedOptionsExpanded()`, same pattern as
`CallWorkspacePanel`'s W113a "Advanced diagnostics"). CPIM removed as a
manually selectable content type (only Plain text/HTML remain) — CPIM
wrapping is applied automatically via the pre-existing, independent
`AppSettings::enableCpim()` global toggle. "Request IMDN:" relabeled
"Delivery receipts:" ("Delivered"/"Read" checkboxes, same underlying
settings).

## 18. Advanced options

See item 17 — transport (Automatic/SIP MESSAGE/MSRP Preferred/MSRP
Required, default Automatic unchanged), content-type (Plain text/HTML),
delivery receipts (Delivered/Read). All persisted exactly as before; only
their visibility/grouping changed.

## 19. Tools messaging inspector

Audited, not extended in this task — see
[tools-messaging-inspector.md](../tools-messaging-inspector.md) for the
full current-state writeup and gap list (missing Message-ID/transaction-ID/
fallback-reason columns, no full raw-body column). `MessagingDiagnosticsPage`'s
Message History table already correctly special-cases IMDN/typing/delivery
states in its display text (`historyStatusText()`) — the best existing
precedent for how a protocol-event row should be labeled once shown, and
proof Tools' visibility was never the problem; only the Client side was.

## 20. LMPE runtime state

Permanently unavailable everywhere. `CallMediaOptions::enableLmpe` (the
field that would actually gate call/media behavior) was already never set
from `SipProfile::enableLmpe` anywhere in the codebase before this task —
confirmed by full-repo grep. This task closed the *appearance* gap
(checkbox/tab/status card looked interactive) and the *persistence* gap
(old config could show a checked checkbox), not a call-behavior gap that
never existed.

## 21. LMPE config migration

`SipProfileManager::loadAllProfiles()` forces `enableLmpe=false` at the
single authoritative load point regardless of the saved `QSettings`
value, logging one redacted warning (profile id only) when the saved
value was `true`. Does not crash, does not affect any other profile field
or any other transport. `SipProfileEditorDialog`'s save path
(`toProfile()`) also always writes `false`, independent of the checkbox
(which is itself disabled and force-unchecked).

## 22–25. SIP MESSAGE / MSRP send/receive

22. **SIP MESSAGE send**: unchanged — `SipManager::sendSipMessage()` /
    `SipMessageComposer::compose()` not touched by this task.
23. **SIP MESSAGE receive**: fixed (item 9) — now routes through
    `routeInboundMessagingPayload()`.
24. **MSRP send**: unchanged.
25. **MSRP receive**: fixed (item 9) — now routes through the same shared
    function instead of calling `appendInbound()` directly.

## 26. Automatic mode

Unchanged — `MessagingTransportPolicy`'s decision engine and
`SipManager::sendSipMessage()`'s transport-selection logic were not
touched. Still the default in the (now-hidden-by-default) transport
selector.

## 27. Fallback

Unchanged — `MessageHistoryEntry::fallbackReason`/`actualTransport` and
their population in `SipManager::sendSipMessage()` untouched.

## 28. Message-ID correlation

Unchanged for plain SIP MESSAGE (uses the SIP `Message-ID` header,
unaffected by CPIM unwrap since that header lives outside the CPIM
envelope). **Known limitation**: CPIM-internal Message-ID/
Disposition-Notification headers (a distinct concept from the outer SIP
header) are not extracted — `CpimInfo` doesn't model them. Documented in
[messaging-content-type-routing.md](../messaging-content-type-routing.md),
not fixed in this task (would require assuming a CPIM-internal header wire
format this project hasn't confirmed).

## 29. Delivery status

Unchanged mechanics (`DeliveryState` enum, `correlateDelivery()`/
`correlateMsrpDelivery()`) — only the *display* side changed (protocol
rows that carry delivery-state updates no longer appear as separate chat
bubbles; the status still updates on the original message's entry exactly
as before).

## 30. Unread behavior

Fixed — `ConversationModel::unreadCountFor()` now uses
`userVisibleHistoryFor()`; an inbound IMDN report or is-composing
notification no longer increments the unread badge. New test:
`unreadCountIgnoresImdnReportsAndTypingNotifications`.

## 31. Conversation preview

Fixed — `ConversationModel::lastMessageFor()` (read by
`ConversationWorkspacePanel` for the list's preview text) now uses
`userVisibleHistoryFor()`; a trailing IMDN report can no longer become the
displayed preview. New test: `lastMessageForSkipsTrailingImdnReport`.

## 32. HTML safety

**Not audited or changed in this task** (Faza 14 of the spec). `text/html`
rendering behavior in `ClientMessagingView` is unchanged from before this
task. Flagged explicitly as an unaddressed item, not silently assumed
safe — a follow-up task should audit this specifically.

## 33. XML parser safety

Not changed — `CpimParser`/`ImdnParser`/`IsComposingParser` themselves are
untouched (only their call sites/wiring changed). Existing parser tests
(`test_cpim_parser`, `test_imdn_parser`, `test_is_composing_parser`) still
pass unchanged, confirming no regression in parsing behavior itself.

## 34. Debug build

PASS — `build/`, clean rebuild, 81/81 CTest.

## 35. Release build

PASS — `build-release/`, clean rebuild, 81/81 CTest.

## 36. CTest

81/81 both configurations (80 baseline + 1 net-new test binary count
unchanged — new test *cases* were added to three existing test binaries,
not new binaries, so the CTest target count stays 81; the internal QTest
function count within those three binaries grew by 15).

## 37. Teste noi

- `tests/test_message_history.cpp`: `appendInboundUnsupportedStoresPlaceholderNotRawBody`,
  `duplicateInboundUnsupportedIsDeduped`, `isProtocolEventTrueForImdnReport`,
  `isProtocolEventTrueForTypingNotification`, `isProtocolEventTrueForUnsupported`,
  `isProtocolEventFalseForPlainMessage`.
- `tests/test_conversation_model.cpp`: `userVisibleHistoryExcludesImdnReports`,
  `userVisibleHistoryExcludesTypingNotifications`,
  `userVisibleHistoryExcludesUnsupportedPayloads`,
  `userVisibleHistoryKeepsPlainMessages`, `historyForStillIncludesProtocolEvents`,
  `lastMessageForSkipsTrailingImdnReport`,
  `unreadCountIgnoresImdnReportsAndTypingNotifications`.
- `tests/test_sip_profile_manager.cpp`: `enableLmpeForcedFalseOnLoadRegardlessOfSavedValue`.

No test was added directly exercising `SipManager::routeInboundMessagingPayload()`
itself — `SipManager` has no existing unit-test harness for its private
CPIM-unwrap-then-classify glue without a live PJSIP stack (consistent with
this project's established testing boundary: `MessageHistoryStore`/
`ConversationModel` are unit tested directly since they're pure Qt; the
`SipManager` methods that call into them are not). No widget-level test
exists for `RttPanel`/`SipProfileEditorDialog`/`CallWorkspacePanel`/
`ClientMessagingView` (none had a test harness before this task either).

## 38. Test manual

**NOT RUN** — no live SIP peer or second client instance available this
session to run the spec's 25-step Alice/Bob scenario. Every plain-SIP-
MESSAGE-path fix was reasoned about via the existing correct IMDN/
is-composing branches (which the new CPIM branch precedes, same detection
logic reused); every MSRP-path fix mirrors `MsrpPayloadDispatcher`'s
already-tested logic. Not declared PASS.

## 39–43. Result codes

- **PASS:** Debug build, Release build, both CTest runs (81/81 each), all
  15 new test cases, code-level verification of every routing/filtering/
  LMPE-disable change against the audit's findings.
- **FAIL:** none.
- **BLOCKED:** none.
- **NOT RUN:** the manual two-peer GUI scenario (item 38); HTML sanitization
  audit (item 32, out of scope, not attempted); Tools inspector column
  additions (item 19, documented as follow-up, not attempted).
- **UNSUPPORTED:** none.

## 44. Regresii

None expected — both CTest runs are clean at 81/81 including every
pre-existing test. The CPIM/MSRP routing change is additive (new
classification branches; the "everything else" fallback path is
unchanged). The Client UI change only relocates/hides existing controls,
doesn't remove functionality (transport/content-type/delivery-receipts are
all still there, just collapsed by default). LMPE changes only affect
UI-surface widgets and profile-load defaults, never gated any real
call/media behavior to begin with.

## 45. Limitări

- CPIM-internal Message-ID/Disposition-Notification headers not extracted
  (item 28).
- Unknown/unrecognized content types still fall through to plain
  `appendInbound()` without the conservative binary-content sniff
  `MsrpPayloadDispatcher` had — in practice this only matters for a
  content type that is both unrecognized AND binary, arriving over either
  transport; not observed as an actual regression since the pre-W113F
  MSRP path had *zero* classification (this is strictly better than
  before), but noted as not fully matching `MsrpPayloadDispatcher`'s more
  defensive behavior. A follow-up could port that sniff into
  `routeInboundMessagingPayload()`.
- HTML sanitization not audited (item 32).
- Tools inspector not extended with new columns (item 19).
- No widget-level tests exist for the four GUI classes touched
  (`RttPanel`, `SipProfileEditorDialog`, `CallWorkspacePanel`,
  `ClientMessagingView`) — verified by code reading + full rebuild only.

## 46. Fișiere modificate

- `CMakeLists.txt` — version 1.6.6 → 1.6.7.
- `src/sip/SipManager.h`/`.cpp` — new `routeInboundMessagingPayload()`,
  CPIM unwrap, shared by both inbound paths.
- `src/sip/MessageHistoryEntry.h` — new `isUnsupportedOrMalformed` field,
  new `isProtocolEvent()` method.
- `src/sip/MessageHistoryStore.h`/`.cpp` — new `appendInboundUnsupported()`.
- `src/gui/panels/messaging/ConversationModel.h`/`.cpp` — new
  `userVisibleHistoryFor()`; `lastMessageFor()`/`unreadCountFor()` updated
  to use it.
- `src/gui/panels/messaging/ClientMessagingView.h`/`.cpp` — Advanced
  disclosure, CPIM removed from content-type combo, relabeled delivery
  receipts, `userVisibleHistoryFor()` used for rendering.
- `src/core/AppSettings.h` — new
  `clientMessagingAdvancedOptionsExpanded()`/`setClientMessagingAdvancedOptionsExpanded()`.
- `src/gui/dialogs/SipProfileEditorDialog.cpp` — LMPE checkbox
  disabled/relabeled/force-unchecked/force-saved-false.
- `src/gui/panels/RttPanel.h`/`.cpp` — LMPE tab replaced with a disabled
  label; removed dead `lmpeMessageSent` signal/`onLmpeSend()`/unused
  widget members.
- `src/gui/panels/CallWorkspacePanel.cpp` — LMPE status card always
  "Unavailable".
- `src/sip/SipProfileManager.cpp` — forces `enableLmpe=false` at load,
  redacted warning log.
- `tests/test_message_history.cpp`, `tests/test_conversation_model.cpp`,
  `tests/test_sip_profile_manager.cpp` — new test cases (see item 37).
- `tests/CMakeLists.txt` — added `CpimParser.cpp` to the shared
  `CALL_SOURCES` list (needed once `SipManager.cpp` started calling
  `CpimParser::parse` directly — caught by a link failure on
  `test_sip_manager`, fixed before the final build).
- `docs/client-messaging-simplified.md`,
  `docs/messaging-content-type-routing.md`, `docs/lmpe-disabled-status.md`,
  `docs/tools-messaging-inspector.md` (new).
- `docs/agent-prompts/W113F-simplify-client-messaging-and-disable-lmpe.md`,
  `docs/agent-results/W113F-simplify-client-messaging-and-disable-lmpe-result.md`
  (new, this file).
- `docs/imdn.md`, `docs/is-composing.md`, `docs/msrp-foundation.md`,
  `docs/client-messaging-workspace.md`, `docs/project-status.md`,
  `docs/release-notes.md` (updated).

## 47. Commituri

Committed together as this task's single commit (version bump + routing
fix + UI simplification + LMPE disable + tests + docs) — see git log on
this branch.

## 48. Git status

Clean except this task's own changes; pre-existing untracked debris
(`Testing/`, two `diagnostics-*.zip` files from an earlier investigation
task) intentionally left untouched.

## 49. Push status

Pushed: `git push -u origin fix/w113f-simplify-client-messaging-and-disable-lmpe`.

## 50. Confirmare fără merge

Confirmed — no merge into `main` or `release`.

## 51. Confirmare pjproject nemodificat

Confirmed — no files under `.deps/pjproject/` or any PJSIP source were
touched.
