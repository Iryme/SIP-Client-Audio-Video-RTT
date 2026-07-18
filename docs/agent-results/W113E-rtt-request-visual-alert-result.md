# Agent Result — Task W113E: RTT Request Visual Alert Parity with Video

See [W113E-rtt-request-visual-alert.md](../agent-prompts/W113E-rtt-request-visual-alert.md)
for the task prompt and
[incoming-media-request-alerts.md](../incoming-media-request-alerts.md)
for the full technical design.

## 1–6. Branch / version

1. **Branch:** `fix/w113e-rtt-request-visual-alert`
2. **Branch de pornire:** `fix/video-latency-and-codec-investigation`
   (not the ticket's nominal `release/w113d-portable-windows-bundle` —
   branched from the investigation task's tip instead so its fixes
   aren't lost; both are ancestors of the same lineage, no divergence)
3. **Versiune veche/nouă:** 1.6.5 → 1.6.6
4. **Application version:** 1.6.6
5. **Backend version:** 1.6.6 (single binary, one `PROJECT_VERSION`)
6. **Frontend/UI version:** 1.6.6 (same binary)

## 7–8. Audited components

7. **Video request component:** `CallWorkspacePanel` (`m_btnRequestVideo`,
   `refreshRequestVideoButton()`, the ctor's blink-timer setup, and every
   `onVideo*`/`onCall*` slot that starts or stops the alert) —
   `src/gui/panels/CallWorkspacePanel.cpp`.
8. **RTT request component:** the same class's `m_btnRequestRtt`,
   `refreshRequestRttButton()`, `onRttRequested()`/`onRttMediaConnected()`/
   `onRttMediaDisconnected()`, plus `RttSession`
   (`src/rtt/RttSession.{h,cpp}`) for the state machine
   (`RttState::RemoteOfferPending` is what an incoming request maps to)
   and its 12s `kNegotiationTimeoutMs` guard.

## 9. Shared infrastructure

New `src/gui/widgets/RequestBlinker.{h,cpp}` — a 500ms repeating `QTimer` +
bool wrapped in `start()`/`stop()`/`isOn()`, both operations idempotent,
`toggled()` signal for the owner to re-apply its own property/QSS. Video's
prior one-off timer+bool pair was replaced with an instance of this class;
RTT gets its own instance. One implementation now backs both alerts.

## 10–11. Color / flashing interval

10. **Color:** shared QSS rule `[videoAlert="true"], [rttAlert="true"]`
    in `src/gui/theme/ThemeManager.cpp` — `#c86b12` background, `#1f1200`
    text, `#f3a43b` border (`#d67d18`/`#ffb14a` on hover). Identical for
    both.
11. **Flashing interval:** 500ms, both driven by `RequestBlinker`'s fixed
    `QTimer::setInterval(500)`.

## 12–13. Start / stop conditions

12. **Start:** video — `SipManager::videoRequested` →
    `onVideoRequested()`. RTT — `SipManager::rttRequested` (RttSession
    `RemoteOfferPending`) → `onRttRequested()`. Both call `.start()` on
    their respective blinker; idempotent, no duplicate-timer guard needed.
13. **Stop:** both — accept (button toggle success), call
    disconnected/failed, call state → Idle/Failed, media
    connected/disconnected, panel reset. RTT additionally stops on
    `rttRequestRejected`/`rttRequestWithdrawn`/`rttNegotiationFailed` —
    signals that exist only for RTT in `SipManager`/`SipCall` (video's
    re-INVITE path has no equivalent to wire).

## 14. Timer ownership

Each `RequestBlinker` instance is a plain (non-pointer) member of
`CallWorkspacePanel` — its internal `QTimer` is destroyed automatically
with the panel, so there is no dangling-timer/use-after-free risk on
widget destruction.

## 15. Duplicate protection

`RequestBlinker::start()` is idempotent — calling it while already running
just re-arms the interval (matches `QTimer::start()`'s own semantics), it
does not create a second timer. No explicit `.isActive()` guard needed at
any call site (same conclusion the pre-existing video code already relied
on).

## 16. Stale-callback protection

`RttSession`'s `rttRequestWithdrawn` handling (already existed pre-W113E,
now also wired to stop the blinker) specifically guards against a peer
cancelling its offer before the user answers; `onRttMediaDisconnected()`
now also resets `m_rttRequested`/stops the blinker unconditionally, so a
disconnect for a request that never reached Active can't leave a stale
"pending" alert running.

## 17. Multiple calls

`SipManager` is single-active-call by architecture (unchanged, out of
scope for this task) — isolation is the same rigorous reset already used
throughout `CallWorkspacePanel`: every call-lifecycle handler that resets
call state also stops both blinkers before the next call's state builds
up.

## 18. Simultaneous video/RTT request

Independent `RequestBlinker` instances driving independent buttons — a
simultaneous incoming video + RTT request shows both buttons flashing at
once, each with its own accept text. No priority ordering was needed;
nothing is hidden.

## 19. Accessibility

Added `toolTip()`, `accessibleName()`, and `accessibleDescription()` to
both `m_btnRequestVideo` and `m_btnRequestRtt` across all three of their
states (normal/accept-pending/active) — neither button had any
accessibility metadata before this task. The alert is not color-only: text
("Request X" → "Accept X"), tooltip, and accessible description all change
together with the flashing property.

## 20. Light/dark theme

Not applicable — `ThemeManager.cpp`'s QSS uses the app's single dark theme
token set already in place for every other `CallCtrlBtn` role; no
light-theme variant exists elsewhere in this file to diverge from.

## 21–23. Builds / tests

21. **Debug build:** PASS — `build/`, clean, 81/81 CTest (80 baseline +
    new `test_request_blinker`).
22. **Release build:** PASS — `build-release/`, clean, 81/81 CTest.
23. **CTest:** 81/81 both configurations.

## 24. Teste noi

`tests/test_request_blinker.cpp` (10 cases): defaults to off; `start()`
turns on immediately and emits `toggled()` synchronously; `start()` is
idempotent; `stop()` turns off and emits `toggled()` only when it was
actually on; `stop()` is idempotent; the real `QTimer` toggles `isOn()` on
successive ticks (via `QSignalSpy::wait()`); restarting after a stop
begins from "on" again.

No test was added directly exercising `CallWorkspacePanel`'s RTT-alert
wiring itself — the panel has no existing unit-test harness (it's a
dense, live-SIP-manager-coupled widget), consistent with how the
pre-existing video-alert code was never unit-tested either; the shared
`RequestBlinker` class is exactly the part of this behavior that's both
new and independently testable, hence the focus there.

## 25. Test GUI manual

**NOT RUN** — no live SIP peer or input-automation tooling available in
this session to drive the two-peer incoming-request scenario (Bob sends
video then RTT requests, confirm identical flashing, accept/reject/
timeout, multi-call isolation) end to end. Same constraint flagged on
every GUI-facing task this session.

## 26–30. PASS / FAIL / BLOCKED / NOT RUN / UNSUPPORTED

- **PASS:** Debug build, Release build, both CTest runs (81/81 each), the
  new `test_request_blinker` suite, code-level verification of every
  start/stop call site against the video pattern.
- **FAIL:** none.
- **BLOCKED:** none.
- **NOT RUN:** the 20-step manual GUI test plan (no live peer/hardware/
  input-automation this session).
- **UNSUPPORTED:** none.

## 31. Regresii

None expected. Video's blink behavior is unchanged in substance — only
its internal representation moved from a one-off `QTimer`+bool pair to a
`RequestBlinker` instance with identical timing (500ms) and identical
start/immediate-on/stop semantics. Both Debug and Release CTest runs
(including all pre-existing tests) pass unchanged.

## 32. Limitări

- No unit test exercises `CallWorkspacePanel`'s RTT-alert wiring directly
  (widget-level, SIP-manager-coupled — no harness exists for it, same as
  video's alert before this task).
- Manual GUI verification NOT RUN (documented above).
- `RttSession`'s 12s negotiation timeout only applies to `LocalOfferPending`
  (an offer *we* made); there is no equivalent bounded wait for
  `RemoteOfferPending` (a request we haven't yet answered) — this matches
  video's existing behavior (which also has no auto-timeout while waiting
  for the local user to click Accept), so it's parity, not a new gap.

## 33. Fișiere modificate

- `CMakeLists.txt` — version 1.6.5 → 1.6.6; added
  `src/gui/widgets/RequestBlinker.cpp`/`.h` to SOURCES/HEADERS.
- `src/gui/widgets/RequestBlinker.h`, `.cpp` (new).
- `src/gui/panels/CallWorkspacePanel.h` — replaced
  `m_videoRequestBlinkTimer`/`m_videoRequestBlinkOn` with
  `RequestBlinker m_videoRequestBlinker`/`m_rttRequestBlinker`.
- `src/gui/panels/CallWorkspacePanel.cpp` — blinker wiring throughout
  (ctor, both toggle handlers, both refresh functions, all call-lifecycle
  slots), three new RTT-only signal connections
  (`rttRequestRejected`/`rttRequestWithdrawn`/`rttNegotiationFailed`),
  accessibility metadata on both request buttons.
- `src/gui/theme/ThemeManager.cpp` — shared `rttAlert`/`videoAlert` QSS
  rule; `acceptRtt` added to the shared base/hover/checked/disabled
  selector group; `rttActive` added to share `videoActive`'s green style.
- `tests/test_request_blinker.cpp` (new), `tests/CMakeLists.txt` (new
  test target).
- `docs/incoming-media-request-alerts.md` (new) — full design writeup.
- `docs/agent-prompts/W113E-rtt-request-visual-alert.md`,
  `docs/agent-results/W113E-rtt-request-visual-alert-result.md` (new,
  this file).
- `docs/call-workspace.md`, `docs/project-status.md`,
  `docs/release-notes.md` (updated).

## 34. Commituri

Committed together as this task's single commit (version bump + RequestBlinker
+ CallWorkspacePanel wiring + ThemeManager QSS + tests + docs) — see git log
on this branch.

## 35. Git status

Clean except this task's own changes; pre-existing untracked debris
(`Testing/`, the two `diagnostics-*.zip` files from the earlier
investigation task) intentionally left untouched.

## 36. Push status

Pushed: `git push -u origin fix/w113e-rtt-request-visual-alert`.

## 37. Confirmare fără merge

Confirmed — no merge into `main` or `release`.

## 38. Confirmare pjproject nemodificat

Confirmed — no files under `.deps/pjproject/` or any PJSIP source were
touched, and no RTT/video SDP negotiation, `requestRtt()`,
`acceptIncomingRttRequest()`, `rejectIncomingRttRequest()`, RTP port
allocation, or state-machine logic was changed — this task only added
UI-side signal connections and button styling/accessibility.
