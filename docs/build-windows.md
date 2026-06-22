# Build — Windows

## Prerequisites

- Visual Studio 2019 or 2022 (Desktop C++ workload)
- CMake 3.16+ (bundled with VS or install separately)
- Qt 6.4+ from the Qt online installer: https://www.qt.io/download
  - Select: Qt 6.x → MSVC 2019 64-bit or MSVC 2022 64-bit
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

## Run

```cmd
build\Release\SIPClient.exe
```

Add Qt bin directory to PATH, or use `windeployqt`:

```cmd
cd build\Release
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

## Known Issues

- PJSIP on Windows requires additional setup (prebuilt DLLs or custom build). Documented when SIP integration is added.
- Qt Multimedia on Windows requires at minimum Windows 10. DirectShow or Windows Media Foundation backend will be used.
