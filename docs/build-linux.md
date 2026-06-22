# Build — Linux

## Prerequisites

```bash
# Ubuntu 22.04+
sudo apt install cmake ninja-build git
sudo apt install qt6-base-dev qt6-multimedia-dev

# Fedora 38+
sudo dnf install cmake ninja-build git
sudo dnf install qt6-qtbase-devel qt6-qtmultimedia-devel
```

Minimum Qt version: 6.4

## Build

```bash
git clone https://github.com/Iryme/SIP-Client-Audio-Video-RTT.git
cd SIP-Client-Audio-Video-RTT

cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6

cmake --build build --parallel
```

## Run

```bash
./build/SIPClient
```

## Debug Build

```bash
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
./build-debug/SIPClient
```

## Run Tests (when available)

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Known Issues

- Qt 6.2 (Ubuntu 22.04 default) may be missing `Qt6MultimediaWidgets`. Use Qt6 from the Qt online installer if the distro version is too old.
- PJSIP will require additional system libraries when SIP integration is added: `libssl-dev`, `libopus-dev`, `libvpx-dev`.
