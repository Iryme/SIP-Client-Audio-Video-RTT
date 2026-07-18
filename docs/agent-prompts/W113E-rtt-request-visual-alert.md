# Agent Prompt — Task W113E: RTT Request Visual Alert Parity with Video

Condensed from the full task spec provided by the project owner in chat
(`W113E-rtt-request-visual-alert.txt`) — this file summarizes the
requirements rather than duplicating every line.

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `fix/w113e-rtt-request-visual-alert`.
- Starting branch: `fix/video-latency-and-codec-investigation` (the
  ad-hoc investigation task the project owner asked for ahead of W113E;
  branched from there rather than the ticket's nominal
  `release/w113d-portable-windows-bundle` so that investigation's fixes
  aren't lost). No merge into `main`/`release`. `pjproject` not modified.

## Objective

Make an incoming RTT request produce the exact same visual alert as an
incoming video request: same color logic, same highlighting, same
flashing/pulsing, same visual priority, same stop conditions, no
duplicated animation/styling logic.

## Versioning

PATCH bump 1.6.5 → 1.6.6 (UI-only change, single-binary app/backend/UI
version, no schema change).

## Approach taken

1. Audited the existing video-request alert via an Explore agent plus
   direct code reading: `m_videoRequestBlinkTimer`/`m_videoRequestBlinkOn`
   (500ms repeating `QTimer`), the `videoAlert` dynamic property + QSS
   rule in `ThemeManager.cpp`, and every start/stop call site across
   `CallWorkspacePanel.cpp`.
2. Extracted that pattern into a new shared, reusable class,
   `src/gui/widgets/RequestBlinker.{h,cpp}` (start/stop/isOn, idempotent,
   no widget/SIP knowledge) — both video and RTT now use their own
   instance of this one class instead of two independently-implemented
   blink timers.
3. Wired RTT's existing `m_rttRequested`/accept-mode state to the new
   blinker at every point video already had one, plus three RTT-only stop
   signals (`rttRequestRejected`/`rttRequestWithdrawn`/`rttNegotiationFailed`)
   that have no video equivalent because the underlying `SipCall`/
   `SipManager` signals only exist for RTT.
4. Added a shared `[rttAlert="true"]` QSS rule identical in color to
   `[videoAlert="true"]`; also fixed two pre-existing style gaps found
   along the way (`acceptRtt` had no QSS rule at all; `rttActive` had no
   distinct "active" style unlike `videoActive`) as part of the same
   centralization pass.
5. Added `toolTip()`/`accessibleName()`/`accessibleDescription()` to both
   Request Video and Request RTT buttons (neither had any before) so the
   alert isn't color-only.
6. Added `tests/test_request_blinker.cpp` — unit tests for the new shared
   helper's start/stop/idempotency/timer-tick behavior.

## Global rules (same discipline as every task this session)

Debug + Release + `ENABLE_PJSIP=ON` builds; all existing tests pass; new
tests for the new behavior; docs; `docs/project-status.md` update;
agent-prompt/agent-result pair; push (no merge); no pjproject changes; no
changes to RTT/video SDP negotiation, `requestRtt()`,
`acceptIncomingRttRequest()`, `rejectIncomingRttRequest()`, RTP port
allocation, or the RTT/video state machines themselves — this task is UI/
state-binding only.

See [W113E-rtt-request-visual-alert-result.md](../agent-results/W113E-rtt-request-visual-alert-result.md)
for the full 38-item final report.
