# Windows Follow-up — Validate W113I Video Stream Guard

Create `fix/w113i-windows-video-stream-guard-validation` from the completed
W113I macOS fix branch. Do not merge it as part of the macOS task.

The common files `src/sip/SipCall.cpp`, `src/sip/VideoStreamGuard.*`, and test
configuration now prevent STOP/START_TRANSMIT unless a PJSIP call has a
matching ACTIVE video media entry with transmit direction and a current stream
index. Rebuild on Windows x64 with the supported MSVC/PJSIP/Qt toolchain and
run Debug and Release CTest. Live-test Camera On/Off with no call, audio-only,
video, no common video codec, hold/resume, repeated toggles, remote BYE,
teardown, and shutdown. Verify DirectShow/GDI camera lifecycle, LMPE disabled,
and no raw protocol XML in Client Messaging.

Windows is still 1.6.8. Only after the Windows build and regression matrix
passes, update Windows resources, runtime reporting, portable metadata, and
artifact names to 1.6.9 and produce the Windows portable bundle. Record any
NOT RUN/BLOCKED gate explicitly; do not infer PASS from macOS results.
