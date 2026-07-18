# Incoming Media Request Alerts (Task W113E)

`CallWorkspacePanel`'s Request Video and Request RTT buttons share one
visual-alert mechanism for "the remote peer wants to add this media to the
call" — same color, same flashing, same start/stop conditions, same
accessibility treatment. This doc describes that shared behavior; see
[call-workspace.md](call-workspace.md) for the rest of `CallWorkspacePanel`.

## Shared infrastructure: `RequestBlinker`

`src/gui/widgets/RequestBlinker.{h,cpp}` is a small, UI/SIP-agnostic helper:
a `QTimer` (500ms, repeating) plus a bool, wrapped in `start()`/`stop()`/
`isOn()`. It has no knowledge of buttons, stylesheets, or call state — it
just tells its owner "the alert should currently be shown" via the
`toggled()` signal.

`CallWorkspacePanel` owns two independent instances,
`m_videoRequestBlinker` and `m_rttRequestBlinker`, each wired to its own
button-refresh function (`refreshRequestVideoButton()` /
`refreshRequestRttButton()`). Before Task W113E, video had its own
one-off `QTimer m_videoRequestBlinkTimer` + `bool m_videoRequestBlinkOn`
pair with no RTT equivalent; W113E extracted that into `RequestBlinker`
and gave RTT its own instance of the exact same class, so the two can't
drift apart in timing or semantics again.

- `start()` — idempotent (safe to call while already running, mirrors
  `QTimer::start()`'s own idempotency: no duplicate timer is created).
  Turns the indicator on **immediately** and emits `toggled()`
  synchronously, so the UI doesn't wait for the first 500ms tick before
  showing the alert.
- `stop()` — idempotent. Always leaves `isOn() == false`; emits
  `toggled()` only if the state actually changed, so callers can't
  accidentally re-apply a stale "on" style after a redundant `stop()`.

## Visual treatment (identical for both media types)

- **Color**: `QPushButton#CallCtrlBtn[videoAlert="true"]` and
  `[rttAlert="true"]` share one QSS rule in `src/gui/theme/ThemeManager.cpp`
  (`#c86b12` background / `#1f1200` text / `#f3a43b` border, `#d67d18` /
  `#ffb14a` on hover) — a single edit updates both.
- **Flashing**: both properties toggle true/false every 500ms via their
  respective `RequestBlinker`, re-polished (`style()->unpolish()`/
  `polish()`) on every toggle.
- **Not color-only**: the button's text changes ("Request Video"/"Request
  RTT" → "Accept Video"/"Accept RTT"), and both buttons now carry a
  `toolTip()`, `accessibleName()`, and `accessibleDescription()` that
  update with state (e.g. *"Incoming RTT (real-time text) request — click
  to accept"* / *"The remote party is requesting to add real-time text to
  this call."*) — screen readers and tooltip users get the same
  information sighted users get from color/text.
- **Active state**: `[callRole="videoActive"]` and `[callRole="rttActive"]`
  now share the same green "active" style too (previously only
  `videoActive` had one; `rttActive`/`acceptRtt` had no QSS rule at all and
  silently fell back to the platform's default `QPushButton` look — fixed
  as part of this same centralization pass).

## Start conditions

- **Video**: `SipManager::videoRequested()` (relayed from
  `SipCall::videoRequested`) → `CallWorkspacePanel::onVideoRequested()` →
  `m_videoRequestBlinker.start()`.
- **RTT**: `SipManager::rttRequested()` (relayed from `SipCall::rttRequested`,
  itself driven by `RttSession` entering `RttState::RemoteOfferPending`) →
  `CallWorkspacePanel::onRttRequested()` → `m_rttRequestBlinker.start()`.

Neither path guards against a duplicate incoming signal creating a second
timer — `RequestBlinker::start()`'s idempotency makes that unnecessary
(matches the pre-existing, already-proven video behavior).

## Stop conditions

Both alerts stop (timer stopped, property reset to `false`, normal style
restored) on:

- **Accept** — the button's own toggle handler, on a successful
  `requestCallVideo()`/`acceptIncomingRtt()` call.
- **Call disconnected** (`onCallDisconnected`) / **call failed**
  (`onCallFailed`) / **call state → Idle/Failed** (`onCallStateChanged`).
- **Media actually connects** (`onVideoMediaConnected`/`onRttMediaConnected`)
  or **disconnects** (`onVideoMediaDisconnected`/`onRttMediaDisconnected`).
- **Panel reset** (`resetStatusCards()`), and implicitly on widget
  destruction (`RequestBlinker` is a plain member of `CallWorkspacePanel`,
  so its `QTimer` is destroyed with the panel — no dangling timer callback
  is possible).

RTT additionally stops on three signals that have no video equivalent,
because the underlying `SipCall`/`SipManager` layer only exposes them for
RTT (see `src/sip/SipManager.h`'s `rttRequestRejected`/`rttRequestWithdrawn`/
`rttNegotiationFailed` — video's re-INVITE path has no matching signals to
wire even if it wanted to):

- `rttRequestRejected` — the local user rejected the request via the
  `MediaRequestDialog` popup's Ignore button (`SipManager::rejectIncomingRtt()`).
- `rttRequestWithdrawn` — the peer cancelled its offer before the user
  answered.
- `rttNegotiationFailed` — the re-INVITE itself failed at the transport/SDP
  level (also covers `RttSession`'s own 12s `kNegotiationTimeoutMs` guard,
  which calls `SipCall::cancelPendingRttRequest()` and transitions to
  `RttState::Failed` — that failure path does not, on its own, emit a
  distinct Qt signal beyond what already triggers `rttNegotiationFailed`
  for local-offer timeouts; see `src/rtt/RttSession.cpp`).

## Multiple calls / duplicate requests

`SipManager` remains single-active-call by architecture (see
[call-state-and-media-model.md](call-state-and-media-model.md)) — there is
exactly one `CallWorkspacePanel` and one pair of blinkers, so "multiple
calls" isolation is the same rigorous state reset used everywhere else in
this panel: `resetStatusCards()`/the call-lifecycle handlers above always
stop both blinkers before a new call's state is built up, so a stale alert
from a previous call can never bleed into the next one. A duplicate
`rttRequested`/`videoRequested` signal for the same still-pending request
does not start a second timer (`RequestBlinker::start()`'s idempotency).

## Simultaneous video + RTT requests

Because video and RTT each own an independent `RequestBlinker` driving an
independent button, a simultaneous incoming video request and RTT request
are shown coherently: both buttons flash at once, each with its own text
("Accept Video" / "Accept RTT") — no priority ordering was needed since
nothing is hidden or overwritten.
