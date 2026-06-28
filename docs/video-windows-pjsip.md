# Video TX Unavailable — Windows PJSIP Build

## Symptom

Local camera preview works (Qt `QCamera` → `QVideoSink` → `QVideoFrame` pipeline), but the
remote peer cannot see video from this device. The log contains:

```
PJSIP video capture backend not active; local transmit unavailable:
PJMEDIA_VIDEO_DEV_HAS_DSHOW=0 — Qt preview is local-only, remote cannot see video from this app
```

The UI shows **"Receive-only video"** or **"Negotiated, no local capture"** instead of "Active"
for the SIP Video status card.

## Root Cause

PJSIP was compiled without the DirectShow video device plugin:

```
PJMEDIA_VIDEO_DEV_HAS_DSHOW=0
```

`PJMEDIA_VIDEO_DEV_HAS_DSHOW` is set in `pjmedia/include/pjmedia-videodev/config.h`. When it is
`0`, the `pjmedia-videodev` library has no Windows capture backend. PJSIP's RTP video stack can
still decode and render incoming video (via the GDI renderer we provide in
`src/media/PjsipGdiRenderer.cpp`), but it cannot open a capture device and therefore cannot
send local camera frames to the remote peer.

## Effect

| Feature | Works |
|---|---|
| Qt local camera preview (`QCamera`) | ✅ |
| Incoming video (remote → local display via GDI) | ✅ |
| Outgoing video (local camera → remote via RTP) | ❌ |

## Fix: Rebuild PJSIP with DirectShow

### Prerequisites

Install the Windows SDK (needed for DirectShow headers):

```
winget install Microsoft.WindowsSDK.10.0.22621
```

Or via Visual Studio Installer → "Desktop development with C++" → add "Windows SDK".

### Steps

1. Open `pjproject/pjmedia/include/pjmedia-videodev/config.h` and verify:
   ```c
   #define PJMEDIA_VIDEO_DEV_HAS_DSHOW  1
   ```
   If it is `0`, set it to `1`.

2. Verify that the PJSIP solution or build script links `strmiids.lib` and `quartz.lib`
   (DirectShow dependency libraries).

3. Rebuild pjproject from the VS2026 developer command prompt:
   ```cmd
   cd pjproject
   nmake -f Makefile-vs2026
   ```
   or use the Visual Studio solution `pjproject-vs14.sln`.

4. Rebuild the SIP client:
   ```cmd
   cmake --build build-registration --config Release
   ```

5. Verify in the log:
   ```
   SipManager: PJSIP video capture available
   ```
   And the UI shows no "⚠ Local preview uses Qt camera" warning in Settings → Video.

### Format Converter Note

Even with DirectShow enabled, PJSIP may fail to encode camera output if `PJMEDIA_HAS_FFMPEG=0`
and `PJMEDIA_HAS_LIBYUV=0`. The DirectShow capture typically outputs `YUY2` (YUYV), but VP8
encoding in PJSIP requires `I420`. Without a format converter, PJSIP cannot encode the frame.

To work around this, either:
- Enable `libyuv` in pjproject (`PJMEDIA_HAS_LIBYUV=1`), or
- Configure the camera to output `I420` directly (not always supported by webcams), or
- Use a codec that accepts `YUY2` natively.

The receive path is unaffected because the GDI renderer converts `I420` → `BGRA` itself.
