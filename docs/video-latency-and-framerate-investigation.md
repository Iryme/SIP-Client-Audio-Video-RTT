# Video Latency & Framerate Investigation (2026-07-18 test session)

Requested by the project owner before starting Task W113E, after a manual
Alice/Bob test session reported "video and framerate problems" and "very
high latency even on a local network." This investigation was scoped as
its own task (branch `fix/video-latency-and-codec-investigation`, off
`release/w113d-portable-windows-bundle`) rather than folded into W113E,
since it's a different area of the app (video/codec pipeline) than
W113E's RTT-alert UI work.

## Data sources

Two Diagnostics Center export bundles from the same test session, both
app v1.6.4 (commit `4d05a04`):

- `diagnostics-20260718-213203.zip` — "Bob"'s profile
- `diagnostics-20260718-213221.zip` — "Alice"'s profile

Both contain `logs.txt`, `diagnostics.json`, `sip_trace.json`, and related
files. No packet capture, no per-frame timing data, and no live
RTCP/video-stream numeric samples are present in either bundle — see
finding 4 below for why that gap itself is significant.

## Findings

### 1. Bob's camera is RDP-redirected

`logs.txt` (Bob): `Local preview device info: ... name='FaceTime HD Camera
(redirected)' driver='dshow'`. This is the same environment characteristic
already documented in this project's operational notes: testing over RDP
means camera devices are redirected through an extra virtual channel
hop before the frame ever reaches DirectShow/PJSIP, which adds latency
that has nothing to do with the app's own code. **Not a code bug** — an
inherent property of testing two clients over RDP sessions on one
physical machine, worth remembering when interpreting any latency
complaint from this kind of test setup specifically.

### 2. This PJSIP build has exactly one usable video codec: VP8

Both sides' startup logs show:

```
CodecManager: video codecs (1 available):
  ENABLED   VP8/102                          priority=230
```

`CMakeLists.txt`'s PJSIP third-party link list (`resample`, `g7221`,
`gsm`, `ilbc`, `speex`, `srtp`, `webrtc`, `webrtc_aec3`, `yuv`, plus the
vcpkg-static `vpx` library) confirms why: **libvpx (VP8) is the only
compiled-in video codec** — there is no OpenH264/H264, no H265, no AV1,
and PJSIP's VP9 support (also part of libvpx) isn't enabled either. VP8 is
a software encoder/decoder only on this build (no hardware acceleration
path), which is inherently more CPU-bound and higher-latency than a
hardware H264 path would be, especially at Alice's negotiated 1280×720 @
30fps. **This is a real architectural constraint, not fixable as a small
patch** — adding H264 support means linking a new third-party codec
library (e.g. OpenH264) into the PJSIP build, which is its own
MAJOR-scale task per `docs/versioning-and-rollout.md` (changes the
negotiated codec capability) and is out of scope here.

### 3. The video-codec preference UI listed codecs that don't exist on this build (FIXED)

`VideoSettings::codecOrder`'s default is `{"H264", "VP8", "VP9", "AV1"}` —
**H264 first** — and `VideoSettingsPanel`'s reorderable codec list
(`populateCodecs()`) populated purely from `VideoQualityManager::knownCodecs()`,
a static aspirational list, with zero indication of which entries this
PJSIP build can actually negotiate. Combined with finding 2, this means:
every fresh install "prefers" a codec (H264) that can never be selected,
silently falls back to VP8, and gives the user no way to discover why
their preferred codec never takes effect — they'd have to compare the
startup log's codec matrix against their settings by hand, exactly as
this investigation had to do.

**Fixed in this task:**
- `CodecManager::applyVideoCodecOrder()` (`src/sip/CodecManager.cpp`) now
  logs a warning when the #1 preferred codec has no matching entry in
  PJSIP's `videoCodecEnum2()`, e.g. `Preferred video codec 'H264' is not
  available in this PJSIP build ... falling back to the next available
  codec in the order`.
- `VideoSettingsPanel::populateCodecs()` (`src/gui/panels/VideoSettingsPanel.cpp`)
  now cross-references `CodecManager::videoCodecs()` and visually marks
  (grayed text + tooltip) any codec name not backed by a real codec on
  this build. The list's save/restore path was changed to read the plain
  codec name from `Qt::UserRole` item data rather than the widget's
  display text, so the new "(not available in this build)" annotation
  can't leak into the saved `codecOrder` setting.

### 4. In-call video FPS/drop indicators never receive real data (confirmed, NOT fixed — see recommendation)

This is the most significant finding and the best explanation for why the
reported "framerate problems" couldn't be quantified from the diagnostics
bundles: **there is currently no live measurement of in-call video FPS,
frame drops, or capture/render latency anywhere in the app.**

Traced the full signal chain:

- `CallWorkspacePanel`'s FPS and Video Drops status cards
  (`onVideoStatsUpdated()`) are wired to `VideoStatistics::statsUpdated`.
- `VideoStatistics::frameProduced()`/`frameDrop()` — the only two places
  that ever update those counters — are called *exclusively* from
  `VideoPanel::onIdlePreviewFrame()`, the **idle Qt-side local-camera
  preview loop** (the "camera preview when not in a call" path).
- `VideoPanel::onVideoMediaConnected()` — fired the moment a real call's
  video media connects — explicitly calls `stopIdlePreview()`, with the
  comment "PJSIP's DirectShow capture locks the camera device." This is
  correct (Qt and PJSIP can't both hold the same capture device), but it
  means **`VideoStatistics` receives zero updates for the entire duration
  of any actual video call.**
- The FPS/Drops cards therefore freeze at whatever value existed the
  instant before the call connected (typically 0, since idle-preview
  startup and the switch to PJSIP capture happen within the same event
  loop tick) and never move again until the call ends and idle preview
  restarts.
- Separately, `DiagnosticsCollector.cpp`'s `videoFps`/`videoBitrateKbps`
  fields (visible in `diagnostics.json`, confirmed in both test bundles:
  `"videoFps": 15`, `"videoBitrateKbps": 256`) are populated directly from
  `VideoQualityManager::current().fps`/`.bitrateKbps` — **the configured
  target**, not a measurement of what the call actually achieved.

In short: every number in the app that looks like a live video performance
metric is either frozen (status cards) or a static configuration echo
(diagnostics export). There is no way, today, for a user or a developer
to confirm from inside the app whether a call is actually running at its
configured fps/bitrate or badly under-delivering — which matches exactly
the position the project owner was in reporting "framerate problems"
with only a diagnostics bundle to go on.

**Not fixed in this task** — a correct fix means polling PJSIP's real
video stream statistics (`pjmedia_vid_stream_get_stat()`, which reports an
`pjmedia_rtcp_stat` per active video stream: packet/byte counts, jitter,
loss — usable to derive a live fps/throughput estimate via periodic
delta sampling) and feeding that into `VideoStatistics`/`CallInfoModel`
while `m_videoActive` is true, replacing or supplementing the idle-preview
path. This touches `SipCall`'s media-state handling, needs a polling timer
with correct start/stop lifecycle (mirroring the existing `m_videoRetryTimer`
pattern), and should ship with its own tests — sized like its own task,
not a rider on this investigation. See Recommendation below.

### 5. Repeated video-window re-attachment — investigated, not a bug

`logs.txt` shows `attachVideoWindows`/`Remote video GDI renderer pointed
at Qt widget` firing many times through the call rather than once at
connect. Cross-referencing timestamps against the surrounding log lines
shows every one of these re-attach events is directly preceded by a
`Camera On requested from CallWorkspacePanel` / `Camera Off requested from
CallWorkspacePanel` log line — i.e. **this is the user manually toggling
Camera On/Off repeatedly during the test**, each toggle correctly
triggering `VideoPanel`'s documented re-attach-after-Camera-On path
(`src/gui/panels/VideoPanel.cpp`, the `CameraController::enabledChanged`
handler). Working as designed, not a source of the reported problem.

### 6. One RTT negotiation timeout observed — investigated, not a bug

`logs.txt` (Alice) shows one `RTT negotiation timed out (state=LocalOfferPending)
after 12000 ms` during the session. This is `RttSession`'s designed
12-second guard (`RttSession::kNegotiationTimeoutMs`) firing because the
peer didn't answer the local RTT offer in time during testing — expected
behavior, not a defect. Relevant context for the upcoming W113E task:
today this timeout produces only a log line and a state transition to
`Failed`, with **no visual indication to the user that anything
happened** — exactly the gap W113E's RTT-alert-parity work is meant to
close.

## Conclusion

The reported video/framerate/latency problems are most plausibly explained
by a combination of:

1. An RDP-redirected camera on one side of this specific test (environment
   artifact, not app-side).
2. A software-only VP8 codec path with no hardware-accelerated alternative
   compiled into this PJSIP build (architectural limitation — a future
   MAJOR-scope task, not a quick fix).
3. A genuine, now-partially-fixed UX gap where the codec preference UI
   silently offered unachievable choices (finding 3, fixed this task).
4. A genuine, unfixed observability gap: the app cannot currently measure
   or display real in-call video FPS/drops/latency at all (finding 4) —
   so future reports of "it feels slow" can't be corroborated or refuted
   from diagnostics alone until that's addressed.

No evidence of a video-pipeline correctness bug (re-attach churn, RTT
timeout) was found beyond the fixed UI/logging gap in finding 3.

## Recommendation (follow-up, not done in this task)

Scope a dedicated task to poll `pjmedia_vid_stream_get_stat()` (or the
pjsua2-level equivalent) for the active call's video stream on a short
interval while `m_videoActive` is true, derive real fps/throughput/loss
from the deltas, and feed that into `VideoStatistics`/`CallInfoModel` so
the Call Workspace's FPS/Video Drops cards and the diagnostics export
report what's actually happening on the wire, not the configured target.
That instrumentation is also the only way to eventually confirm whether
adding a hardware H264 path (finding 2) meaningfully improves latency.
