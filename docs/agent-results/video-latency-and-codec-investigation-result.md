# Agent Result — Video Latency & Framerate Investigation

See [video-latency-and-codec-investigation.md](../agent-prompts/video-latency-and-codec-investigation.md)
for the task prompt and
[video-latency-and-framerate-investigation.md](../video-latency-and-framerate-investigation.md)
for the full technical findings. This file is the task-report summary.

## Branch / version

- **Branch:** `fix/video-latency-and-codec-investigation`
- **Branch de pornire:** `release/w113d-portable-windows-bundle`
- **Versiune veche/nouă:** 1.6.4 → 1.6.5

## Data analyzed

Two Diagnostics Center bundles from the reported test session
(`diagnostics-20260718-213203.zip` = "Bob", `diagnostics-20260718-213221.zip`
= "Alice"), both app v1.6.4, commit `4d05a04`.

## Findings summary

1. **Bob's camera was RDP-redirected** ("FaceTime HD Camera (redirected)",
   `driver='dshow'`) — inherent test-environment latency, not a code issue.
2. **This PJSIP build has exactly one usable video codec: VP8**
   (software-only; no H264/OpenH264, H265, AV1, or VP9 compiled in,
   confirmed by both sides' startup logs: `CodecManager: video codecs (1
   available): ENABLED VP8/102`). A real architectural constraint;
   adding hardware H264 support is a separate MAJOR-scope task, not
   attempted here.
3. **Fixed** — the codec-preference UI/logging gave zero indication that
   the default preference order's #1 choice (H264) doesn't exist on this
   build, so users had no way to know why their preferred codec silently
   never took effect.
4. **Confirmed, not fixed** — there is no live in-call video FPS/drop/
   latency telemetry anywhere in the app; the Call Workspace's status
   cards and the diagnostics export both trace back to
   `VideoStatistics`, which is only fed by the idle (out-of-call) camera
   preview loop and receives zero updates once `VideoPanel::onVideoMediaConnected()`
   stops that loop for the call's duration. Recommended as a dedicated
   follow-up task (poll `pjmedia_vid_stream_get_stat()`).
5. **Investigated, not a bug** — repeated video-window re-attach log lines
   during the test correlate exactly with manual Camera On/Off toggling in
   the session, not a pipeline defect.
6. **Investigated, not a bug** — one RTT negotiation timeout (12000 ms) is
   `RttSession`'s designed guard firing because the peer didn't answer in
   time; relevant context for W113E (today this timeout has no visual
   indication for the user, which W113E's work addresses).

## Fixes applied

- `src/sip/CodecManager.cpp` — `applyVideoCodecOrder()` now logs a warning
  when the preferred order's top choice has no matching PJSIP codec.
- `src/gui/panels/VideoSettingsPanel.cpp` — the codec reorder list now
  marks (gray text + tooltip) codec names not backed by a real PJSIP
  codec on this build; the save path now reads the codec name from
  `Qt::UserRole` item data instead of the display text, so the new
  annotation can't corrupt the saved `codecOrder` setting.

## Not fixed (explicit follow-up recommendation)

- Real in-call video FPS/drop/latency telemetry (finding 4) — needs its
  own scoped task: poll `pjmedia_vid_stream_get_stat()` on a timer while
  `m_videoActive`, feed `VideoStatistics`/`CallInfoModel` from real
  deltas, add tests.
- H264/hardware-codec support (finding 2) — MAJOR-scope build/third-party
  dependency change.

## Debug build

PASS — `build/`, clean rebuild, 80/80 CTest.

## Release build

PASS — `build-release/`, clean rebuild, 80/80 CTest.

## Tests noi

None added. The `CodecManager` change only executes inside `#ifdef
HAVE_PJSIP`, which the existing `test_codec_manager` suite explicitly
`QSKIP`s (it covers stub-mode only). The `VideoSettingsPanel` change has
no existing widget-test harness to extend. Verified by code reading plus
the two full Debug/Release rebuilds above.

## Test GUI manual

NOT RUN — no live SIP peer or camera hardware available in this session
to re-run the Alice/Bob scenario interactively; this investigation worked
entirely from the two already-captured diagnostics bundles and the
source code they pointed to.

## PASS / FAIL / BLOCKED / NOT RUN / UNSUPPORTED

- **PASS:** Debug build, Release build, both CTest runs (80/80 each), the
  two applied fixes (verified by code reading + successful compilation).
- **FAIL:** none.
- **BLOCKED:** none.
- **NOT RUN:** live GUI re-test of the original Alice/Bob scenario (no
  peer/hardware available); real in-call video telemetry (not attempted,
  scoped as follow-up, not a test that was skipped).
- **UNSUPPORTED:** none.

## Regresii

None expected — both changes are additive (a new log line; a new visual
annotation + a bugfix to what the codec-order list saves, which now saves
*more* correctly than before, not less).

## Limitări

- Root causes 1 and 2 (RDP camera, VP8-only codec) are environmental/
  architectural, not resolved by this task and not resolvable as a small
  patch.
- Finding 4 (no real in-call video telemetry) remains open — this
  investigation's most actionable conclusion is that any future latency/
  framerate complaint can't be corroborated from inside the app until
  that's addressed.

## Fișiere modificate

- `CMakeLists.txt` — version 1.6.4 → 1.6.5.
- `src/sip/CodecManager.cpp` — codec-availability warning.
- `src/gui/panels/VideoSettingsPanel.cpp` — codec-list annotation + save-path fix.
- `docs/video-latency-and-framerate-investigation.md` (new) — full findings.
- `docs/agent-prompts/video-latency-and-codec-investigation.md`,
  `docs/agent-results/video-latency-and-codec-investigation-result.md`
  (new, this file).
- `docs/project-status.md`, `docs/release-notes.md` (updated).

## Commituri

Committed together as this task's single commit (version bump + fixes +
docs) — see git log on this branch.

## Git status

Clean except this task's own changes; pre-existing untracked debris
(`Testing/`, the two `diagnostics-*.zip` files that were the input to this
investigation) intentionally left untouched and not added to git.

## Push status

Pushed: `git push -u origin fix/video-latency-and-codec-investigation`.

## Confirmare fără merge

Confirmed — no merge into `main` or `release`.

## Confirmare pjproject nemodificat

Confirmed — no files under `.deps/pjproject/` or any PJSIP source were
touched. Only PJSIP headers were *read* (`config.h` for the version
number, `vid_stream.h` for the stats API surface used in the
recommendation) — no PJSIP source file was edited.
