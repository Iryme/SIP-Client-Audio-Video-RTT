# Task W097 — Active is-composing — Result

1. **Branch**: `feature/w097-is-composing`

2. **Branch de pornire**: `feature/w096-imdn-foundation`

3. **Fișiere modificate/adăugate**:
   - Noi: `src/sip/IsComposingGenerator.h`/`.cpp`,
     `src/sip/TypingIndicatorController.h`/`.cpp`, `docs/is-composing.md`,
     `docs/agent-prompts/W097-is-composing.md`,
     `docs/agent-results/W097-is-composing-result.md`,
     `tests/test_typing_indicator_controller.cpp`.
   - Modificate: `src/sip/IsComposingInfo.h`, `src/sip/IsComposingParser.cpp`,
     `src/sip/SipMessageComposer.h`/`.cpp`, `src/core/AppSettings.h`,
     `src/sip/MessageHistoryEntry.h`, `src/sip/MessageHistoryStore.h`/`.cpp`,
     `src/sip/SipManager.cpp`, `src/gui/panels/MessagingDiagnosticsPage.h`/`.cpp`,
     `src/sip/MessagingEvent.h`, `src/sip/MessagingEventStore.cpp`,
     `src/sip/InteropTraceExporter.h`/`.cpp`, `CMakeLists.txt`,
     `tests/CMakeLists.txt`, `tests/test_is_composing_parser.cpp`,
     `tests/test_message_history.cpp`, `tests/test_sip_message_foundation.cpp`,
     `docs/sip-message.md`, `docs/messaging-event-store.md`,
     `docs/windows-trace-json-export.md`, `docs/messaging-diagnostics.md`,
     `docs/project-status.md`.

4. **Cum se generează active/idle/gone**: `IsComposingGenerator::generate(state,
   refreshSeconds, contentType)` (pure Qt/text, no PJSIP dependency) builds
   an RFC 3994 `<isComposing>` XML document with `<state>`, an optional
   `<refresh>` (only when `refreshSeconds > 0`), and an optional
   `<contenttype>` (only when non-empty — set to the compose box's current
   Content-Type selection when sending `active`). Returns an empty string
   for `state == Unknown`. `SipMessageComposer::composeIsComposing()` wraps
   this into a `ComposedSipMessage` with
   `contentType = "application/im-iscomposing+xml"`, sent via the unchanged
   `SipManager::sendSipMessage()` path.

5. **Debounce/rate limiting**: `TypingIndicatorController` (pure
   Qt/`QTimer`) tracks a local phase — `Stopped`/`Active`/`Idle`.
   `onTextChanged(true)` while already `Active` **only restarts the idle
   timer** (debounce) — it never re-emits `active` per keystroke. `active`
   is re-sent **only** by a periodic refresh timer (throttle), keeping a
   long typing burst from producing more than one notification per
   `typingRefreshSeconds`. `stop()` (message sent or editor closing) emits
   `gone` **at most once** per composing session — guarded by the `Stopped`
   phase check, so calling it twice, or when nothing was in progress, is a
   no-op. This directly satisfies "nu trimite active/active/active/active."

6. **Configurare timere**: three new `AppSettings` (seconds):
   `typingRefreshSeconds` (default 60, keep-alive resend cadence while
   still typing), `typingIdleSeconds` (default 15, inactivity before
   `idle`), `typingGoneDelaySeconds` (default 30, delay after `idle`
   before `gone` if typing never resumes). Plus two policy toggles, both
   default **ON**: `enableIsComposing`, `autoTypingNotifications`.

7. **Cum funcționează indicatorul UI**: two places. (a) Message History
   rows: an inbound typing notification is its own row, rendered as
   `✍️ typing...` / `Idle` / `Gone` (per the task's example). (b) A live
   label next to the compose box reflects the typing state of whoever is
   currently in the "To" field; on `active` it shows `✍️ typing...` and
   starts an expiry `QTimer` for `typingRefreshSeconds` — if no further
   notification arrives before it fires, the label clears automatically
   (satisfying "indicatorul trebuie să dispară automat după refresh dacă nu
   mai vin notificări"); `idle`/`gone` clear/update it immediately. Both are
   driven by the existing `MessageHistoryStore::entryAppended` signal — no
   polling, UI thread never blocked.

8. **Modificările în export JSON**: `generatedIsComposing` (bool),
   `receivedIsComposing` (bool), `typingState` (string), `typingRefresh`
   (string), `typingTimeout` (string) added to every event in both
   `MessagingEventStore::exportToJson()` and `InteropTraceExporter`'s
   per-event object — purely additive, mapped from the already-parsed
   `MessagingTraceEntry::isComposing` (no new parsing).
   `InteropTraceExporter::kSchemaVersion` **stays 2**.

9. **Ce rămâne neimplementat**:
   - Presence, XCAP — not implemented, per the task's explicit rule.
   - MSRP — remains fully disabled everywhere.
   - No per-conversation typing indicator — the compose UI is a single flat
     box, not per-contact conversations, so only the currently-entered
     recipient's typing state is shown live.
   - No retry for a failed typing-notification send.

10. **Teste rulate**: full build with `ENABLE_PJSIP=ON` (`nmake`, clean —
    zero errors) followed by `ctest --output-on-failure`: **100% tests
    passed, 0 failed, 47 total** (46 from before this task, plus the new
    `test_typing_indicator_controller` suite).

11. **Limitări**:
    - `TypingIndicatorController`'s timer-driven transitions are exercised
      via test-only `trigger*Timeout()` hooks rather than waiting real
      wall-clock seconds (same pattern as `SipManager::scheduleRefresh`) —
      this proves the state-machine logic deterministically but does not
      exercise actual `QTimer` firing latency.
    - `IsComposingGenerator`/`TypingIndicatorController` were validated
      against hand-built fixtures and round-trip through the existing
      parser; no real-world Linphone capture of live typing notifications
      was available.
    - The live typing indicator only reflects the peer currently in the
      "To" field — switching recipients does not re-fetch/replay history
      for the new peer's last-known typing state.

12. **Commituri** (4, on `feature/w097-is-composing`):
    - `c2e36ee` feat(is-composing): implement notification generator
    - `9a6ca8a` feat(is-composing): automatic typing state machine
    - `15cac22` feat(ui): typing indicator
    - `00c7b7c` test(is-composing): add interoperability tests
    - (this commit) docs(is-composing): document workflow

13. **git status**: clean after this commit (confirmed in the final chat
    report).

14. **Push status**: pushed to `origin/feature/w097-is-composing` after this
    commit, per the mandatory workflow. No merge into main/release.
