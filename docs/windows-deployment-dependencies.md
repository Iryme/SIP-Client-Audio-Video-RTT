# Windows Deployment Dependencies (Portable Bundle)

What SIPClient.exe actually depends on at runtime on Windows, and how each
dependency gets into the portable bundle produced by
[scripts/package-windows.ps1](../scripts/package-windows.ps1). See
[windows-portable-bundle.md](windows-portable-bundle.md) for how to run that
script.

## Qt 6 (dynamic)

Linked components: `Core`, `Widgets`, `Gui`, `Network`, `Multimedia`,
`MultimediaWidgets`, `Svg`, plus `Qt6::CorePrivate` (for the real-ZIP
Diagnostics Bundle export, if the installed Qt build exposes it — see
`CMakeLists.txt`'s `HAVE_QT_ZIP_WRITER` guard).

Deployed by `windeployqt.exe --release --compiler-runtime --no-translations`,
run against the staged copy of `SIPClient.exe` (not the build-tree copy).
Plugin directories the app actually exercises:

| Directory | Why it's needed |
|---|---|
| `platforms/` (`qwindows.dll`) | Required to create any Qt window at all — its absence is the classic "this application failed to start" Qt error. |
| `multimedia/` (`windowsmediaplugin.dll`, `ffmpegmediaplugin.dll`) | Qt Multimedia backend selection for camera/mic capture and playback — see the FFmpeg row below. |
| `styles/`, `imageformats/`, `iconengines/` | Widget theming, `.svg`/`.png` icons throughout the GUI (nav rail, status cards). |
| `tls/` | `QNetworkAccessManager`/TLS backend — used by MSRP-over-TLS and any HTTPS diagnostics/XCAP paths. |
| `networkinformation/` | `QNetworkInformation` (reachability signal used by registration retry backoff). |

`windeployqt` decides which of these actually ship based on what the built
binary references; the packaging script only *verifies* `platforms/` and
`multimedia/` are present afterward (hard failure if missing) and warns
(non-fatal) if the others are absent, since a given Qt build may not need
all of them.

## FFmpeg (Qt Multimedia's backend on Windows)

`avcodec-61.dll`, `avformat-61.dll`, `avutil-59.dll`, `swresample-5.dll`,
`swscale-8.dll` — Qt 6.11's Windows Multimedia backend is FFmpeg-based.
These are pulled in automatically by `windeployqt --compiler-runtime`'s own
dependency walk of `Qt6Multimedia.dll`/`ffmpegmediaplugin.dll`; the packaging
script does not copy them by hand.

## PJSIP / pjproject (static)

Linked as static `.lib` files from
`.deps/pjsip-msvc-install-release/bin/*.lib` (see `CMakeLists.txt`'s
`ENABLE_PJSIP` block) — `pjlib`, `pjlib-util`, `pjnath`, `pjmedia`,
`pjmedia-audiodev`, `pjmedia-videodev`, `pjmedia-codec`, `pjsip`,
`pjsip-simple`, `pjsip-ua`, `pjsua-lib`, `pjsua2`, plus the third-party codec
libs (`resample`, `g7221`, `gsm`, `ilbc`, `speex`, `srtp`, `webrtc`,
`webrtc_aec3`, `yuv`) and `vpx` (VP8, from vcpkg's static triplet). **None of
these produce a runtime DLL** — there is nothing to deploy for PJSIP beyond
what's already inside `SIPClient.exe`.

OpenSSL/zlib used by PJSIP's TLS/SRTP paths are likewise linked statically
through the PJSIP build — no separate `libssl-3-x64.dll`/`libcrypto-3-x64.dll`
to ship.

## Microsoft Visual C++ runtime

Strategy **A** (local deployment) is applied via
`windeployqt --compiler-runtime`, which places `vcruntime140.dll`,
`vcruntime140_1.dll`, `msvcp140.dll`, etc. directly next to `SIPClient.exe` —
the target machine needs no system-wide CRT install for normal use.

Strategy **B** (`vc_redist.x64.exe`) is *additionally* included in the bundle
by default (`-IncludeVcRedist`, on by default) for environments with a policy
against loose CRT DLLs, or as a fallback if a future Qt/toolchain upgrade
changes what `--compiler-runtime` captures. `vc_redist.x64.exe` is the same
file already tracked from prior releases (`build-release/vc_redist.x64.exe`,
originally obtained from Microsoft's official download) — the script never
downloads a new one at package time.

## Windows system DLLs (not shipped)

`kernel32.dll`, `user32.dll`, `gdi32.dll`, `advapi32.dll`, `ws2_32.dll`,
`d3d11.dll`/`dxgi.dll` (Qt's RHI backend), `crypt32.dll`, `bcrypt.dll`, and
similar OS-provided DLLs are never copied into the bundle — they're part of
Windows itself. `scripts/package-windows.ps1`'s dependency audit
(`dumpbin /dependents` over every binary in staging) allow-lists these by
name so the audit doesn't false-positive on them, and fails the build if it
finds any *other* DLL reference that isn't already present in staging.

## What's deliberately NOT in the bundle

- `SIPClient.pdb` — ships separately as `*-symbols.zip`
  (`-IncludeSymbols`), not inside the public portable zip.
- Source code, build cache, intermediate object files, test binaries.
- Any personal config, credential store, packet capture, or diagnostics
  bundle from the build machine.
