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

## PJSIP Integration (optional)

PJSIP is an optional dependency. The project builds and runs without it using a **stub SIP backend**. To enable PJSIP, pass `-DENABLE_PJSIP=ON`.

### Building PJSIP on Windows

1. Download PJSIP source from https://github.com/pjsip/pjproject
2. Open `pjproject-vs14-vs2015.sln` (or generate with CMake) in Visual Studio
3. Build the `pjsua2-lib` target in Release/x64
4. Note the output directory (typically `pjproject\lib\`)

### Configuring with PJSIP

```powershell
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DQt6_DIR=F:/Programs/Qt/6.11.1/msvc2022_64/lib/cmake/Qt6 `
  -DENABLE_PJSIP=ON `
  -DPJSIP_DIR=C:/pjproject
```

The `cmake/FindPJSIP.cmake` module searches for:
- Headers under `$PJSIP_DIR/include/` (looks for `pjsua2.hpp`)
- Libraries under `$PJSIP_DIR/lib/`

When PJSIP is found, `HAVE_PJSIP` is defined and the real PJSIP backend is compiled. The status bar will show `SIP: PJSIP/pjsua2 (ready)` instead of `SIP: Stub SIP backend (ready)`.

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
