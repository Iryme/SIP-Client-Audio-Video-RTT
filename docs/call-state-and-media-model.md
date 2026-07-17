# Call State and Media Model (Task W113)

## `CallInfoModel`: single source of truth

`CallInfoModel` (`src/gui/panels/call/CallInfoModel.{h,cpp}`) is a plain
`QObject` aggregating every piece of the active call's display state: call
state, remote identity, presence text, duration, mute/hold flags,
video/RTT connected+requested flags, selected media, negotiated audio/video
codec, RTP stats, video FPS/drop stats, and device names.

It is deliberately **fed via setters**, never by reaching into `SipManager`
itself — the same "dependency-free, fed externally" pattern already used by
`ConversationListModel`/`CallHistoryListModel` (Task W111/W112). This is
why it has its own guiless unit test
(`tests/test_call_info_model.cpp`) with no `SipManager`/pjsua2 dependency:
the test constructs a model and calls setters with synthetic data.

`CallWorkspacePanel` owns one `CallInfoModel` instance and updates it in
parallel with its own widget state, in the same places it already updates
`StatusCard`s. This satisfies the roadmap's "UI must not infer active media
from a button's checked-state" requirement: the model is the source of
truth other consumers (tests, future diagnostics) can read without touching
any widget.

## Selected vs. negotiated vs. actual media — three distinct concepts

- **Selected**: what the user asked for when the call was placed
  (`CallMediaOptions`, captured in `CallWorkspacePanel::placeCall()` from
  the request-video/request-RTT button state at call-launch time).
  `SipCall` itself has no notion of this — it only ever knows what was
  negotiated — so this has to be captured at the entrypoint, which is
  exactly what consolidating every call-launch path onto `placeCall()`
  (see [call-workspace.md](call-workspace.md)) makes possible.
- **Negotiated**: what the SDP exchange actually produced
  (`SipManager::activeAudioCodecInfo()`/`activeVideoCodecInfo()`, read
  through to `SipCall`'s post-negotiation state). A call can select
  video+RTT and negotiate audio-only; the two are never conflated.
- **Actual**: what's currently, live, measured — RTP/RTCP stats
  (`SipManager::currentRtpStats()`) and the video pipeline's own FPS/drop
  counters (`VideoStatistics::statsUpdated`).

## Packet-loss / video-drop-count fix

Before this task, `MainWindow`'s single `cardPacketLoss` status card was
written to by two independent, unrelated data sources:
`SipManager::rtpStatsChanged` (an RTCP packet-loss **percentage**) and
`VideoStatistics::statsUpdated` (the local video pipeline's own frame-drop
**count**, unrelated to network loss). Whichever signal fired last won,
silently overwriting the other's unit on the same card.

`CallWorkspacePanel` fixes this by keeping them on two separate cards:
`m_cardPacketLoss` (RTP/RTCP loss %, from `currentRtpStats()`) and
`m_cardVideoDrops` (local video frame drops/sec, from
`VideoStatistics::statsUpdated`). `CallInfoModel` mirrors the same
separation (`rtpStats()` vs. `videoDropsPerSecond()`), and
`tests/test_call_info_model.cpp::packetLossAndVideoDropsStaySeparate`
guards against the two being merged again.

## Stats-after-teardown guard

The roadmap explicitly warns against calling `getInfo()` after a call has
torn down without a guard. This was already correctly handled before this
task, at `SipCall::mediaRtpStats()`
(`src/sip/SipCall.cpp`, guarded by `isTeardownState()` + try/catch) — this
task reuses `SipManager::currentRtpStats()`/`rtpStatsChanged` exclusively
and adds no new direct `getInfo()` call site, so that existing guard
coverage is preserved rather than duplicated or bypassed.

## Camera / capture ownership

The hardware camera (`CameraController::instance()`) remains the single
owner of camera enable/disable state, unchanged by this task —
`CallWorkspacePanel`'s camera-toggle button and `VideoPanel`'s preview both
read/write through the same singleton, exactly as `MainWindow`'s inline
code did before.
