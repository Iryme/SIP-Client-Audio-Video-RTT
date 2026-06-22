# Media — Audio and Video

**Status:** NOT STARTED

## Audio

- Input: microphone device (selected in Media tab)
- Output: speaker/headset device (selected in Media tab)
- Codecs: OPUS, G.711 (PCMU/PCMA), G.722 planned
- PJSIP handles RTP/RTCP, jitter buffer, echo cancellation

## Video

- Input: camera device (selected in Media tab)
- Codecs: VP8, VP9, H.264 planned
- Remote video rendered in `VideoPanel` via Qt Multimedia video surface
- Local preview rendered in the PiP widget inside `VideoPanel`

## Device Selection

Media tab in the center area info tabs will contain:
- Microphone selector (`QComboBox`)
- Speaker selector (`QComboBox`)
- Camera selector (`QComboBox`)
- Video resolution selector
- Audio level meters

## Statistics

Statistics tab shows:
- Jitter (ms)
- Packet loss (%)
- RTT latency (ms)
- Audio bitrate (kbps)
- Video bitrate (kbps)
- RTP stream status (active/inactive)

These values are also mirrored in the status bar.

## Implementation Notes

- PJSIP `CallMediaInfo` provides stream statistics from `CallInfo`.
- Statistics are polled on a timer (1 s interval) during active calls.
- Audio level meters use PJSIP signal level APIs or Qt audio probe.
- Video surface integration uses `QVideoSink` / `QVideoWidget` (Qt 6 Multimedia).
- Device enumeration uses `QMediaDevices::audioInputs()`, `audioOutputs()`, `videoInputs()`.
