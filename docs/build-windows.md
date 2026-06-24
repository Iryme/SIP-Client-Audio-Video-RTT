# Build — Windows

## Prerequisites

- Visual Studio 2019, 2022, or 2026 (Desktop C++ workload)
- CMake 3.16+ (bundled with VS or install separately)
- Qt 6.4+ from the Qt online installer: https://www.qt.io/download
  - Select: Qt 6.x → MSVC 2019/2022 64-bit
  - Components: Qt Core, Qt Widgets, Qt Multimedia, Qt MultimediaWidgets

## Build (Developer Command Prompt)

```cmd
git clone https://github.com/Iryme/SIP-Client-Audio-Video-RTT.git
cd SIP-Client-Audio-Video-RTT

cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64

cmake --build build --config Release --parallel
```

Replace `6.x.x` and `msvc2022_64` with your installed Qt version and kit.

## NMake Build (tested setup — VS 2026 dev shell)

```powershell
# Open VS 2026 Developer PowerShell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" `
  -Arch amd64 -SkipAutomaticLocation

cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DQt6_DIR=F:/Programs/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6

cd build
nmake
```

## Run

```cmd
build\SIPClient.exe
```

Add Qt bin directory to PATH, or use `windeployqt`:

```cmd
cd build
C:\Qt\6.x.x\msvc2022_64\bin\windeployqt.exe SIPClient.exe
SIPClient.exe
```

## Build with Qt Creator

1. Open `CMakeLists.txt` in Qt Creator.
2. Select the Qt 6 MSVC 64-bit kit.
3. Click Configure → Build.

## Debug Build

```cmd
cmake --build build --config Debug --parallel
build\Debug\SIPClient.exe
```

## Build with Tests

```cmd
cmake -B build ... -DBUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure
```

Or with NMake:

```powershell
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DQt6_DIR=... -DBUILD_TESTS=ON
nmake
.\tests\test_sip_manager.exe -o results.xml,xml
```

---

## PJSIP Integration

PJSIP is optional only when `ENABLE_PJSIP=OFF`, which is the default. When `ENABLE_PJSIP=ON` is requested, configuration must find a real PJSIP/pjsua2 install prefix or CMake fails. This prevents accidental stub-backend validation.

### Build And Install PJSIP With MSVC 2022

The PJSIP project supports Visual Studio project builds on Windows. Its CMake build/install support is available in current pjproject sources and is useful for producing a clean install prefix with `include`, `bin`, `lib/cmake/Pj`, and `lib/pkgconfig`. The PJSIP documentation still marks CMake support experimental on Windows, so keep the exact source revision and options in your validation notes.

Open an x64 Native Tools Command Prompt for VS 2022:

```cmd
cd /d C:\src
git clone --depth 1 https://github.com/pjsip/pjproject.git
cd pjproject

cmake -S . -B build-msvc2022 -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_INSTALL_PREFIX=C:\SDKs\pjsip-msvc2022 ^
  -DPJ_SKIP_EXPERIMENTAL_NOTICE=ON ^
  -DBUILD_TESTING=OFF ^
  -DPJMEDIA_WITH_AUDIODEV_WMME=ON ^
  -DPJMEDIA_WITH_AUDIODEV_NULL=ON

cmake --build build-msvc2022 --config Debug --parallel
cmake --install build-msvc2022 --config Debug
```

If your Windows SDK does not provide `phoneaudioclient.h`, disable WASAPI and use WMME/null audio:

```cmd
cmake -S . -B build-msvc2022 -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_INSTALL_PREFIX=C:\SDKs\pjsip-msvc2022 ^
  -DPJ_SKIP_EXPERIMENTAL_NOTICE=ON ^
  -DBUILD_TESTING=OFF ^
  -DPJMEDIA_WITH_AUDIODEV_WASAPI=OFF ^
  -DPJMEDIA_WITH_AUDIODEV_WMME=ON ^
  -DPJMEDIA_WITH_AUDIODEV_NULL=ON
```

Expected install-prefix checks:

```cmd
dir C:\SDKs\pjsip-msvc2022\include\pjsua2.hpp
dir C:\SDKs\pjsip-msvc2022\bin\pjsua2.lib
dir C:\SDKs\pjsip-msvc2022\lib\cmake\Pj\PjConfig.cmake
```

### Local Task 22B Prefix

This workspace validated PJSIP with the installed Visual Studio 2026/MSVC environment because VS 2022 is not installed on this machine. The PJSIP source and install prefix were kept out of git under `.deps`:

```text
Source:  .deps\pjproject
Commit:  469aa47
Prefix:  .deps\pjsip-msvc-install
Options: PJMEDIA_WITH_AUDIODEV_WASAPI=OFF, PJMEDIA_WITH_AUDIODEV_WMME=ON, PJMEDIA_WITH_AUDIODEV_NULL=ON, BUILD_TESTING=OFF
```

### Video Capture and VPX Codec Support (Task 25B)

To enable DirectShow video capture and VP8/VP9 codecs, three additional steps are required before building pjproject.

#### 1. Build libvpx via vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install libvpx:x64-windows-static
```

This produces `vpx.lib` and `vpx/vpx_encoder.h` under `C:\vcpkg\installed\x64-windows-static\`.

#### 2. Obtain DirectShow BaseClasses headers

The DShow capture backend (`pjmedia-videodev`) needs `streams.h` and related BaseClasses headers. These ship with the Windows SDK samples:

- Path: `C:\Program Files (x86)\Windows Kits\10\Samples\<ver>\multimedia\directshow\baseclasses`

Copy the `baseclasses` directory to `.deps\pjproject\pjmedia\src\pjmedia-videodev\baseclasses` (or set `DSHOW_BASECLASSES_DIR` in your cmake configure). pjproject's DShow backend lists `baseclasses` as a relative include path.

Alternatively, build the BaseClasses static lib separately and point PJSIP at it.

#### 3. Fix `config_auto.h.cm` (pjproject upstream bug)

pjproject's CMake template `pjmedia/include/pjmedia-codec/config_auto.h.cm` is missing the VPX entry. Without this fix, `PJMEDIA_HAS_VPX_CODEC` is never written into the installed `config_auto.h` and all VPX codec code compiles out as empty stubs even when `PJMEDIA_WITH_VPX_CODEC=ON`.

Add before the closing `#endif` in `config_auto.h.cm`:

```c
/* VPX (VP8/VP9) video codec */
#ifndef PJMEDIA_HAS_VPX_CODEC
#cmakedefine01 PJMEDIA_HAS_VPX_CODEC
#endif

/* OpenH264 video codec */
#ifndef PJMEDIA_HAS_OPENH264_CODEC
#cmakedefine01 PJMEDIA_HAS_OPENH264_CODEC
#endif
```

This fix is already applied in `.deps/pjproject` in this workspace.

#### 4. Configure pjproject with video flags

```powershell
cmake -S .deps\pjproject -B .deps\pjproject-build `
  -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_INSTALL_PREFIX=".deps\pjsip-msvc-install" `
  -DPJMEDIA_WITH_VIDEODEV_DSHOW=ON `
  -DPJMEDIA_WITH_VPX_CODEC=ON `
  -DVPX_INCLUDE_DIR="C:\vcpkg\installed\x64-windows-static\include" `
  -DVPX_LIBRARY="C:\vcpkg\installed\x64-windows-static\lib\vpx.lib" `
  -DPJMEDIA_WITH_AUDIODEV_WASAPI=OFF `
  -DPJMEDIA_WITH_AUDIODEV_WMME=ON `
  -DPJMEDIA_WITH_AUDIODEV_NULL=ON `
  -DBUILD_TESTING=OFF

cmake --build .deps\pjproject-build --config Debug --parallel
cmake --install .deps\pjproject-build --config Debug
```

#### 5. Configure the app with VPX hints

`PjConfig.cmake` calls `find_dependency(VPX)` at package load time. If VPX is not found, `find_package(Pj CONFIG QUIET)` fails silently and the fallback path searches for `pj.lib` (which does not exist — the real file is `pjlib.lib`), causing 362 unresolved externals. Pass VPX paths explicitly:

```powershell
cmake -S . -B build-pjsip-real `
  -DENABLE_PJSIP=ON `
  -DPJSIP_DIR=".deps\pjsip-msvc-install" `
  -DVPX_INCLUDE_DIR="C:\vcpkg\installed\x64-windows-static\include" `
  -DVPX_LIBRARY="C:\vcpkg\installed\x64-windows-static\lib\vpx.lib" `
  -DBUILD_TESTS=ON `
  -DCMAKE_PREFIX_PATH="F:\Programs\Qt\6.11.1\msvc2022_64"
```

Expected startup log on success:
```
[INFO] [MEDIA] PJSIP video support: ENABLED (PJMEDIA_HAS_VIDEO=1)
[INFO] [MEDIA] PJSIP video devices (5 total):
[INFO] [MEDIA]   [0] "OBS Virtual Camera"  driver=dshow  dir=1
[INFO] [MEDIA] CodecManager: video codecs (1 available):
[INFO] [MEDIA]   ENABLED   VP8/102                          priority=230
```

Note: `LNK4098: defaultlib 'LIBCMT' conflicts` is expected — `vpx.lib` (release static from vcpkg) links against `LIBCMT` while the debug app links `LIBCMTD`. The build succeeds; the warning is harmless.

### Configure The App With PJSIP

```cmd
cmake -S . -B build-pjsip-real ^
  -DENABLE_PJSIP=ON ^
  -DPJSIP_DIR=C:\SDKs\pjsip-msvc2022 ^
  -DBUILD_TESTS=ON ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64

cmake --build build-pjsip-real --config Debug --parallel
ctest --test-dir build-pjsip-real -C Debug --output-on-failure
```

For the Task 22B local prefix, the configure command was:

```cmd
cmake -S . -B build-pjsip-real ^
  -DENABLE_PJSIP=ON ^
  -DPJSIP_DIR=F:\Project\Iryme\SIP-Client-Audio-Video-RTT\.deps\pjsip-msvc-install ^
  -DBUILD_TESTS=ON ^
  -DCMAKE_PREFIX_PATH=F:\Programs\Qt\6.11.1\msvc2022_64
```

Successful configure output must include:

```text
PJSIP found - building with real SIP backend
```

When PJSIP is found, `HAVE_PJSIP` is defined and the real PJSIP backend is compiled. `SipManager::backendName()` returns `PJSIP/pjsua2`, and the status bar shows `SIP: PJSIP/pjsua2 (ready)`.

### Missing PJSIP Is Fatal With ENABLE_PJSIP=ON

This command must fail if the prefix is missing or invalid:

```cmd
cmake -S . -B build-pjsip-missing-check ^
  -DENABLE_PJSIP=ON ^
  -DBUILD_TESTS=ON ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
```

Expected error:

```text
PJSIP not found. Pass -DPJSIP_DIR=<path> or set the PJSIP_DIR environment variable to your PJSIP installation root.
```

### Stub mode (default — ENABLE_PJSIP=OFF)

When PJSIP is not available the application runs in stub mode:
- `SipManager::isPjsipAvailable()` returns `false`
- `SipManager::backendName()` returns `"Stub SIP backend"`
- `initialize()` succeeds without side effects
- All SIP panels remain visible but calls and registration are non-functional
- A `WARN` log entry is emitted: `"PJSIP unavailable — running stub SIP backend"`

---

## Known Issues

- Qt Multimedia on Windows requires at minimum Windows 10. DirectShow or Windows Media Foundation backend is used.
- PJSIP on Windows may require additional SSL libraries (`libssl`, `libcrypto`). Link against the same OpenSSL version used to build PJSIP.
