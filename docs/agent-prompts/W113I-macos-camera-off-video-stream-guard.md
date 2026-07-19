# W113I — macOS Camera Off Video Stream Guard

Base: `feature/w113g-macos-arm64-bundle` at `e82a496`.
Branch: `fix/w113i-macos-camera-off-video-stream-guard`.

Fix the macOS 1.6.8 SIGABRT observed when Camera Off reached
`pjmedia_vid_stream_pause` through `Call::vidSetStream` although the call had
no created, active, transmitting PJSIP video stream. Camera On/Off must remain
safe with no call, audio-only SDP, no common codec, inactive/error media,
invalid indices, repeated input, BYE, and teardown. Local Qt preview state must
remain independent from negotiated PJSIP video state. Do not edit pjproject or
add codecs.

Release macOS as 1.6.9/build 169 while keeping every Windows version surface
at 1.6.8. Add automated guard/regression coverage, native ARM64 Debug/Release
builds, full CTest, packaging/audits/signing/relocation smoke tests, and honest
live-test statuses. Because `SipCall` is common code, provide a separate
Windows rebuild/regression prompt; do not perform that work on this branch.

The complete source request is the W113I task supplied to the agent on
2026-07-19; this checked-in summary records its scope without environment or
credential data.
