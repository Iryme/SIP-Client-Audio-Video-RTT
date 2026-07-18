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

Use the checklist in
[windows-macos-interoperability-test.md](windows-macos-interoperability-test.md)
before declaring cross-platform media support released.

