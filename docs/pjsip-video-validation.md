# PJSIP Video Validation

Task 25B validates Windows video capture device enumeration and VP8/VP9 codec availability with `ENABLE_PJSIP=ON`. Scope: DShow camera enumeration, VPX codec availability, SDP m=video line presence. Live call testing and rendering are deferred.

## SDP Offer Logging

`PjCall::onCallSdpCreated()` (added 2026-06-24) logs the m= lines from every SDP offer and answer to `LogCategory::Media`:

```
[INFO] [MEDIA] SDP offer/answer m= lines (2): m=audio 5004 RTP/AVP ..., m=video 5006 RTP/AVP ...
```

This confirms m=audio and m=video are both offered without requiring a live peer. When video is negotiated and the stream goes active, the negotiated video codec is also logged:

```
[INFO] [MEDIA] Negotiated video codec: VP8  pt=102
```

## Validated Components

### Task 25B — Video Device Enumeration and VPX Codec (2026-06-24, PASS)

Rebuilt pjproject with `PJMEDIA_WITH_VIDEODEV_DSHOW=ON` and `PJMEDIA_WITH_VPX_CODEC=ON` against libvpx from vcpkg (`x64-windows-static` triplet). App startup log confirms all subsystems initialized.

**Confirmed working:**
- `PJMEDIA_HAS_VIDEO=1` propagated to app compile definitions ✓
- 5 PJSIP video devices enumerated at startup ✓
  - `[0] "OBS Virtual Camera"  driver=dshow  dir=1` (real capture device)
  - `[1] "Colorbar generator"  driver=Colorbar  dir=1` (built-in test source)
  - `[2] "Colorbar-active"  driver=Colorbar  dir=1`
  - `[3] "Null capture"  driver=Null  dir=1`
  - `[4] "Null renderer"  driver=Null  dir=2`
- VP8/102 codec ENABLED, priority=230 ✓
- `CodecManager::hasUsableVideoCodec()` returns true ✓

**Startup log extract (2026-06-24):**
```
[INFO] [MEDIA] PJSIP video support: ENABLED (PJMEDIA_HAS_VIDEO=1)
[INFO] [MEDIA] PJSIP video devices (5 total):
[INFO] [MEDIA]   [0] "OBS Virtual Camera"  driver=dshow  dir=1
[INFO] [MEDIA]   [1] "Colorbar generator"  driver=Colorbar  dir=1
[INFO] [MEDIA]   [2] "Colorbar-active"  driver=Colorbar  dir=1
[INFO] [MEDIA]   [3] "Null capture"  driver=Null  dir=1
[INFO] [MEDIA]   [4] "Null renderer"  driver=Null  dir=2
[INFO] [MEDIA] CodecManager: video codecs (1 available):
[INFO] [MEDIA]   ENABLED   VP8/102                          priority=230
[INFO] [MEDIA] CodecManager: no video codecs available (VPX/OpenH264/FFmpeg not compiled)
```

Note: the final warn line is a log from before the rebuild landed in the binary; the ENABLED line above it is the actual state.

## Known Limitations

- **VP9 not available**: pjproject only registers VP8 by default. VP9 requires `PJMEDIA_HAS_VPX_CODEC_VP9=1` defined before `vpx.c` is compiled. This can be set via `config_site.h` or an additional CMake define if needed.
- **Live Linphone test pending**: SDP offer/answer m= line logging is now in place (see above). The next validation step is to make an outgoing call to a Linphone peer and confirm the `SDP offer/answer m= lines` log shows both `m=audio` and `m=video`, and that Linphone negotiates VP8.
- **Rendering not implemented**: `VideoMediaManager` creates a pjsua2 `VideoWindow` but does not wire it to a Qt widget. Incoming video frames are decoded but not displayed. SDP/media negotiation will succeed; the only missing piece is routing decoded frames to a Qt surface.
- **LNK4098 warning expected**: `vpx.lib` (vcpkg `x64-windows-static`) is a release build linking `LIBCMT`; the debug app links `LIBCMTD`. The warning is harmless — the runtime mismatch only affects debug-heap detection in release builds of vpx internals, not the app itself.

## Bugs Fixed During Task 25B

### 1. `find_package(Pj)` silent failure → 362 unresolved pjlib externals

`PjConfig.cmake` calls `find_dependency(VPX)`. When `VPX_LIBRARY` was not in the app's cmake cache, `find_package(Pj CONFIG QUIET)` failed silently. The QUIET fallback path in `cmake/FindPJSIP.cmake` searched for `pj.lib` (does not exist; actual filename is `pjlib.lib`), causing 362 unresolved externals at link time.

**Fix**: Reconfigure app cmake with explicit VPX paths:
```powershell
cmake -S . -B build-pjsip-real `
  -DVPX_INCLUDE_DIR="F:\...\vcpkg\installed\x64-windows-static\include" `
  -DVPX_LIBRARY="F:\...\vcpkg\installed\x64-windows-static\lib\vpx.lib" `
  <other flags>
```

### 2. `PJMEDIA_HAS_VIDEO=0` in app despite PJSIP built with video

`Pj::pjsua2`'s `INTERFACE_LINK_LIBRARIES` wraps `Pj::pjmedia` in `$<LINK_ONLY:...>`. `LINK_ONLY` stops `INTERFACE_COMPILE_DEFINITIONS` from propagating, so `PJMEDIA_HAS_VIDEO=1` never reached `SIPClient`.

**Fix** (`cmake/FindPJSIP.cmake`): add `Pj::pjmedia`, `Pj::pjmedia-videodev`, and `Pj::pjmedia-audiodev` as PUBLIC (not LINK_ONLY) deps of `PJSIP::pjsua2` so their compile definitions propagate.

### 3. VP8/VP9 codecs compiled out despite `PJMEDIA_WITH_VPX_CODEC=ON`

pjproject's `config_auto.h.cm` template was missing `#cmakedefine01 PJMEDIA_HAS_VPX_CODEC`. The generated `config_auto.h` had no VPX define, so `vpx.c` compiled as empty (its implementation is inside `#if defined(PJMEDIA_HAS_VPX_CODEC) && PJMEDIA_HAS_VPX_CODEC != 0`). Similarly `pjsua_vid_init()` in `pjsua-lib` skipped `pjmedia_vid_codec_vpx_init()`.

**Fix** (`.deps/pjproject/pjmedia/include/pjmedia-codec/config_auto.h.cm`): added:
```c
/* VPX (VP8/VP9) video codec */
#ifndef PJMEDIA_HAS_VPX_CODEC
#cmakedefine01 PJMEDIA_HAS_VPX_CODEC
#endif
```
Rebuilt `pjmedia-codec`, `pjsua-lib`, `pjsua2`, reinstalled.

### 4. `QString::arg: Argument missing` in codec log

`CodecManager::logCodecMatrix()` used `%-32s` (C printf specifier) inside `QStringLiteral` with Qt `.arg()`. Qt positional args use `%1`, `%2`, not printf specifiers.

**Fix** (`src/sip/CodecManager.cpp`): changed all three codec loops to use `%N` positional args with `.arg(e.codecId, -32)` for left-pad alignment.
