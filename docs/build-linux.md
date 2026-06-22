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

## Build with Tests

```bash
cmake -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

---

## PJSIP Integration (optional)

PJSIP is an optional dependency. The project builds and runs without it using a **stub SIP backend**. To enable PJSIP, pass `-DENABLE_PJSIP=ON`.

### Building PJSIP on Linux

```bash
# Install build dependencies
sudo apt install libssl-dev libopus-dev libvpx-dev libsrtp2-dev

# Clone and build PJSIP
git clone https://github.com/pjsip/pjproject.git
cd pjproject
./configure --prefix=/usr/local \
  --enable-shared \
  --with-opus=/usr \
  --with-vpx=/usr
make dep && make
sudo make install
```

### Configuring with PJSIP

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_PJSIP=ON \
  -DPJSIP_DIR=/usr/local
cmake --build build --parallel
```

Or if using the system pkg-config path (no `-DPJSIP_DIR` needed):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_PJSIP=ON
cmake --build build --parallel
```

The `cmake/FindPJSIP.cmake` module searches:
- Headers: `pjsua2.hpp` in `/usr/local/include`, `/usr/include`, or `$PJSIP_DIR/include`
- Libraries: `libpjsua2`, `libpjsua`, `libpjsip`, etc. in standard lib paths

When PJSIP is found, `HAVE_PJSIP` is defined and the real PJSIP backend is compiled.

### Stub mode (default — ENABLE_PJSIP=OFF)

When PJSIP is not available the application runs in stub mode:
- `SipManager::isPjsipAvailable()` returns `false`
- `SipManager::backendName()` returns `"Stub SIP backend"`
- `initialize()` succeeds without side effects
- GUI remains fully functional; only SIP calls and registration are unavailable
- A `WARN` log entry is emitted: `"PJSIP unavailable — running stub SIP backend"`

---

## Known Issues

- Qt 6.2 (Ubuntu 22.04 default) may be missing `Qt6MultimediaWidgets`. Use Qt6 from the Qt online installer if the distro version is too old.
- On Wayland, audio device enumeration via `QMediaDevices` may return fewer devices than on X11. Set `QT_QPA_PLATFORM=xcb` to force X11 if needed.
- Camera hot-plug detection is not yet wired (`QMediaDevices::videoInputsChanged` signal handling deferred).
