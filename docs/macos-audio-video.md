# macOS Audio and Video

Qt Multimedia provides device discovery and idle camera preview. PJSIP 2.17
is built with CoreAudio, AVFoundation capture, Metal rendering, VideoToolbox,
libyuv, and WebRTC AEC enabled. Windows DirectShow/GDI code is excluded.

The W113G build host exposed CoreAudio to the native test process and PJSIP
initialized successfully. No physical camera was available to the non-GUI
test process; PJSIP enumerated its colorbar devices. No external SIP account
or reference client was available, so real registration, two-way audio,
camera permission prompts/LED, video negotiation/rendering, and RTT alongside
media are explicitly not certified by this build pass.

The private PJSIP build contained no H.264/VPX/FFmpeg codec, so its runtime
video-codec enumeration was empty even though AVFoundation/Metal devices were
compiled. Production video calls require at least one compatible PJSIP video
codec and a live interoperability pass. This does not affect Qt camera preview.

## Camera state and PJSIP video state (W113I)

Camera hardware/Qt preview state is deliberately independent of negotiated
call media. Camera Off always stops the local preview through
`CameraController`/`VideoPanel`. It reaches PJSIP STOP_TRANSMIT only when the
call is not tearing down and `CallInfo` contains a matching ACTIVE video media
entry with transmit direction and a currently created video stream index.
Camera On applies the symmetric rule for START_TRANSMIT. Audio-only, missing
codec, inactive/error media, post-BYE, and repeated actions are benign no-ops
at the PJSIP boundary; the UI does not infer that video is active from the
camera toggle.

Use the checklist in
[windows-macos-interoperability-test.md](windows-macos-interoperability-test.md)
before declaring cross-platform media support released.
