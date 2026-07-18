# Active is-composing (RFC 3994)

**Task W100** — [MSRP Foundation](msrp-foundation.md) (branch
`feature/w100-msrp-foundation`) reuses `IsComposingParser` unchanged for
`application/im-iscomposing+xml` bodies carried inside an MSRP SEND —
updating the same typing-indicator history rows this document describes,
no duplicated parsing.

**Task W113F correction** — this doc previously said MSRP-carried
is-composing went through `MsrpPayloadDispatcher`. That class was never
actually wired into the production MSRP receive path; see the same
correction note in [imdn.md](imdn.md) and
[messaging-content-type-routing.md](messaging-content-type-routing.md)
for the actual fix.
**Task W097** — Added in branch `feature/w097-is-composing`, built on top of
[IMDN Foundation (W096)](imdn.md), [SIP MESSAGE Foundation
(W092)](sip-message.md), and the read-only is-composing *parsing* support
introduced by W090's `IsComposingParser`/`IsComposingInfo` (see
[Messaging Diagnostics](messaging-diagnostics.md)). **MSRP remains
completely disabled** — this task only adds typing-indicator generation and
its local state machine over the existing SIP MESSAGE path.
**Task W111** — [Client Messaging Workspace](client-messaging-workspace.md)
gives every conversation in the new `ClientMessagingView` its own
`TypingIndicatorController` instance (`ConversationModel::
typingControllerFor()`), lazily created and never shared across peers —
composing in one conversation can never start or expire a typing session
belonging to another (see `tests/test_conversation_model.cpp`
`typingControllerForIsIndependentAcrossPeers`). No changes to the
controller or generator themselves.

## Overview

Prior to W097 this client could only *parse* an is-composing notification it
happened to receive (diagnostics-only). It never generated one, and the
compose editor had no concept of "the user is typing."

W097 adds:

- **Generation** (`IsComposingGenerator`) — builds RFC 3994
  `application/im-iscomposing+xml` bodies for `active`/`idle`/`gone`.
- **Local typing state machine** (`TypingIndicatorController`) — pure
  Qt/QTimer, decides *when* to announce active/idle/gone as the user types
  in the compose editor, with debounce (no duplicate "active" per keystroke)
  and throttle (periodic keep-alive resend via a refresh timer, not a
  notification storm).
- **Reception** — an inbound typing notification updates Message History as
  its own row; `MessagingEventStore`/Messaging Diagnostics already receive
  it unchanged via the existing raw-trace pipeline (Task W090) — nothing
  duplicated there.
- **UI** — Message History renders `✍️ typing...` / `Idle` / `Gone` rows,
  and a small live indicator next to the compose box reflects the current
  recipient's typing state, auto-clearing if no further notification
  arrives within the refresh window.
- **Config** — `Enable is-composing` / `Auto typing notifications` (both
  default **ON**), plus three timer knobs (`typingRefreshSeconds`,
  `typingIdleSeconds`, `typingGoneDelaySeconds`).

## Architecture

```
Outbound (compose editor)
--------------------------
QPlainTextEdit::textChanged
      ▼
MessagingDiagnosticsPage::onBodyTextChangedForTyping()
      ▼
TypingIndicatorController::onTextChanged(nonEmpty)
      │  pure state machine (Stopped -> Active -> Idle -> Stopped);
      │  debounce: repeated typing while Active only restarts the idle
      │  timer, never re-emits "active"; throttle: "active" is re-sent only
      │  by the periodic refresh timer, not per keystroke
      ▼
TypingIndicatorController::sendIsComposingRequested(state, refreshSeconds)
      ▼
MessagingDiagnosticsPage::onSendIsComposingRequested()
      │  gated on AppSettings::enableIsComposing() &&
      │  AppSettings::autoTypingNotifications()
      ▼
SipMessageComposer::composeIsComposing()      — pure builder
      │  built via IsComposingGenerator::generate()
      ▼
SipManager::sendSipMessage()                   — same send path as W092/W096, unchanged
      │  logs synthetic outbound trace (feeds diagnostics as an is-composing event)
      ▼
SipAccount::sendMessage()                      — PJSIP, unchanged

Sending the actual message, or closing the page, calls
TypingIndicatorController::stop() — sends "gone" if a composing session was
in progress (no-op, duplicate-suppressed, otherwise).

Inbound
-------
Inbound SIP MESSAGE (pjsua2 onInstantMessage)
      ▼
SipManager::onAccountInstantMessageReceived()
      │
      ├─ Content-Type == application/im-iscomposing+xml?
      │     │ yes
      │     ▼
      │  IsComposingParser::parse(body)          — Task W090, unchanged
      │     ▼
      │  MessageHistoryStore::appendInboundTyping(...)   — own history row
      │
      └─ (unrelated to the independent raw-trace tap: PjsipTraceModule ->
          SipTraceLogger -> MessagingDiagnosticsStore -> MessagingEventStore
          already captures the same inbound notification for Messaging
          Diagnostics, exactly as it did before this task — no new parsing
          or duplicated logic added to that pipeline)
```

## `IsComposingGenerator`

`src/sip/IsComposingGenerator.h/.cpp` — pure Qt/text, no PJSIP dependency,
fully unit-testable.

```cpp
static QString generate(IsComposingInfo::State state,
                        int refreshSeconds = 0,
                        const QString &contentType = QString());
```

- Returns an empty string for `state == Unknown` (nothing meaningful to
  generate).
- `<refresh>` is included only when `refreshSeconds > 0` (typically only for
  `active`, per RFC 3994 §5 — left to the caller; `idle`/`gone` are sent
  with `refreshSeconds = 0`).
- `<contenttype>` is included only when non-empty — the task's "include
  contenttype if known" is satisfied by passing the compose box's current
  Content-Type selection when sending `active`.
- `contentType` is XML-escaped.

`IsComposingInfo`/`IsComposingParser` (Task W090, extended here) also gained
a `contentType` field so a generated notification round-trips through the
existing parser.

## `TypingIndicatorController` — debounce/rate limiting

`src/sip/TypingIndicatorController.h/.cpp` — pure Qt (`QTimer`), no
PJSIP/network dependency. States: `Stopped` (nothing sent / after "gone"),
`Active`, `Idle`.

| Trigger | Behavior |
|---|---|
| `onTextChanged(true)` while `Stopped` or `Idle` | Real transition — emits `active`, (re)starts the refresh and idle timers. |
| `onTextChanged(true)` while `Active` | **Debounce** — only restarts the idle timer; never re-emits `active`. |
| Refresh timer fires (still `Active`) | **Throttle** keep-alive — re-emits `active` at the configured cadence, restarts the refresh timer. |
| Idle timer fires (still `Active`) | Emits `idle`, stops the refresh timer, starts the gone-delay timer. |
| Gone-delay timer fires (still `Idle`) | Emits `gone`, returns to `Stopped`. |
| `stop()` (message sent / editor closing) | Emits `gone` **once** — guarded by the `Stopped` check, so calling `stop()` twice (or when nothing was in progress) never double-sends. |

This directly satisfies the task's "nu trimite active/active/active/active"
requirement: a keystroke burst produces exactly one `active` (plus, for a
burst long enough to cross a refresh interval, one keep-alive resend per
interval — never one per keystroke).

Test-only hooks (`triggerRefreshTimeout()`/`triggerIdleTimeout()`/
`triggerGoneTimeout()`) let unit tests exercise every transition
deterministically without waiting real wall-clock seconds — the same
pattern `SipManager::scheduleRefresh()` already uses for registration-timer
tests.

## Timer configuration

Three `AppSettings` knobs (`src/core/AppSettings.h`), seconds:

| Setting | Key | Default | Meaning |
|---|---|---|---|
| `typingRefreshSeconds` | `messaging/typingRefreshSeconds` | 60 | How often `active` is re-sent while the user keeps typing (RFC 3994 keep-alive + throttle). |
| `typingIdleSeconds` | `messaging/typingIdleSeconds` | 15 | Seconds of no typing before `idle` is sent (debounce window). |
| `typingGoneDelaySeconds` | `messaging/typingGoneDelaySeconds` | 30 | Seconds after `idle` before `gone` is sent if typing never resumes. |

Two policy toggles, both default **ON** (is-composing carries no message
content, only a typing indication — no privacy reason to default it off,
unlike W096's Displayed IMDN):

| Setting | Key | Default |
|---|---|---|
| Enable is-composing | `messaging/enableIsComposing` | ON |
| Auto typing notifications | `messaging/autoTypingNotifications` | ON |

Both are exposed as checkboxes in the Messaging Diagnostics page's "Send SIP
MESSAGE" group.

## UI: typing indicator

Two places render typing state:

1. **Message History rows** — an inbound typing notification is its own
   row; the Status column shows `✍️ typing...` / `Idle` / `Gone` (matching
   the task's example) via `historyStatusText()`.
2. **Live indicator** — a small label next to the compose box reflects the
   typing state of whoever is currently in the "To" field (this page has
   one flat compose box, not a per-conversation view, so the indicator is
   scoped to the current recipient). On an `active` notification it shows
   `✍️ typing...` and starts an expiry timer for `typingRefreshSeconds`; if
   no further notification arrives before that timer fires, the indicator
   clears automatically (`onTypingIndicatorExpired()`) — satisfying "the
   indicator must disappear automatically after refresh if no more
   notifications come." An `idle` notification shows `Idle` and cancels the
   expiry timer; a `gone` notification (or expiry) clears the label.

All of this is driven by the existing `MessageHistoryStore::entryAppended`
signal and `QTimer` — no polling, UI thread never blocked.

## Diagnostics surfacing (`MessagingEvent`) / JSON export

`MessagingEvent` gained four additive fields, populated in
`mapFromTraceEntry()` purely from the already-parsed
`MessagingTraceEntry::isComposing` (Task W090's `IsComposingParser`, no new
parsing):

- `generatedIsComposing` (bool) — this event's body is an outbound typing notification.
- `receivedIsComposing` (bool) — this event's body is an inbound typing notification.
- `typingState` (string) — `active`/`idle`/`gone`/`unknown`.
- `typingRefresh` (string) — RFC 3994 `<refresh>` value, verbatim.
- `typingTimeout` (string) — non-standard `<timeout>` value, verbatim.

Exported by both `MessagingEventStore::exportToJson()` and
[`InteropTraceExporter`](windows-trace-json-export.md) — purely additive;
`InteropTraceExporter::kSchemaVersion` stays 2.

## What remains diagnostic-only / out of scope

- **MSRP** — never activated by this task.
- **Presence / XCAP** — not implemented, as required.
- No cross-conversation typing indicator — this client's compose UI is a
  single flat box, not per-contact conversations, so only the currently
  entered recipient's typing state is shown live.
- No retry for a failed typing-notification send — `sendSipMessage()`'s
  normal failure handling applies; the local state machine does not retry.

## Tests

`tests/test_is_composing_parser.cpp` — `IsComposingGenerator` generation +
round-trip through `IsComposingParser` (active/idle/gone), refresh omission
when `<= 0`, rejection of `Unknown`, `<contenttype>` inclusion + escaping.

`tests/test_typing_indicator_controller.cpp` — full state-machine coverage:
first keystroke sends `active`; repeated typing while `Active` does not
resend (debounce); refresh timeout resends `active` (throttle) and is a
no-op when not `Active`; idle timeout transitions to `Idle` and is a no-op
when not `Active`; gone-delay timeout sends `gone` after `Idle` and is a
no-op when not `Idle`; resuming typing after `Idle` sends `active` again;
`stop()` sends `gone` at most once (duplicate suppression) and is a no-op
when nothing was in progress; refresh-seconds config value is reflected in
the emitted payload; an empty-text change is a no-op.

`tests/test_sip_message_foundation.cpp` — `SipMessageComposer::composeIsComposing()`
body content and `Unknown`-state rejection, plus a diagnostics-mapping test
verifying a generated notification's `generatedIsComposing`/`typingState`/
`typingRefresh` fields once it flows through the unmodified W090/W091
pipeline.

`tests/test_message_history.cpp` — `appendInboundTyping()` field mapping and
duplicate-notification dedup (same fingerprint mechanism as W093's
plain-message dedup and W096's IMDN-report dedup).

`tests/test_messaging_diagnostics_store.cpp`'s pre-existing is-composing
coverage is an unmodified regression check — the parsing path
(`IsComposingParser`) is untouched except for the additive `contentType`
field.

Run: `cmake --build build --target all` (with `ENABLE_PJSIP=ON`,
`BUILD_TESTS=ON`) then `ctest --output-on-failure` from `build/`.
