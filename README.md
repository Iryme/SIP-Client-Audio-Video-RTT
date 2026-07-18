# SIP Client — Audio / Video / RTT

A generic, cross-platform SIP multimedia desktop client with a professional dark GUI.

**Platforms:** Windows 10/11, macOS 13+ (Apple Silicon), Linux (Ubuntu 22.04+, Fedora 38+)
**GUI:** Qt 6 C++
**Build:** CMake 3.16+
**Status:** Active development — real PJSIP backend; platform validation varies by feature

---

## Feature summary

| Feature | Status |
|---|---|
| Qt 6 GUI skeleton | IMPLEMENTED |
| Dark professional theme | IMPLEMENTED |
| SIP profile management | IMPLEMENTED |
| SIP registration | IMPLEMENTED; live environment required |
| Audio calls | IMPLEMENTED; macOS live interop pending |
| Video calls | IMPLEMENTED; platform/codec limitations documented |
| RFC 4103 RTT | IMPLEMENTED; macOS live interop pending |
| LMPE messaging | UNAVAILABLE / HARD-DISABLED |
| ETSI emergency foundations | IMPLEMENTED; test/lab scope |

---

## Quick Start

### Prerequisites

- Qt 6.4+ (Core, Widgets, Gui, Multimedia, MultimediaWidgets)
- CMake 3.16+
- C++17 compiler (MSVC 2019+, GCC 11+, Clang 14+)

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/SIPClient
```

See [docs/build-linux.md](docs/build-linux.md), [docs/build-windows.md](docs/build-windows.md),
and [docs/macos-arm64-build.md](docs/macos-arm64-build.md) for detailed instructions.

### macOS ARM64 bundle

The native Release packaging workflow is `scripts/package-macos.sh`. It
produces a relocatable, signed `SIP Client.app`, ZIP, SHA-256, manifest, and
optional DMG; see [macOS deployment](docs/macos-deployment.md).

### Portable Windows bundle

For testing on a Windows machine without Qt/Visual Studio installed, see
[docs/windows-portable-bundle.md](docs/windows-portable-bundle.md) — build with
`scripts/package-windows.ps1`, then copy the resulting `-portable.zip` and
unzip/run `SIPClient.exe`.

---

## Repository Layout

```
src/
  app/          Application bootstrap
  gui/
    panels/     NavRail, Sidebar, Call, Video, RTT, Diagnostics
    widgets/    Status bar
  core/         Logger, AppSettings
  sip/          SIP stack (placeholder — PJSIP planned)
  media/        Audio/video (placeholder)
  rtt/          RFC 4103 RTT (placeholder)
  etsi/         Optional ETSI modules (placeholder)
resources/
  styles/       QSS dark theme
docs/           All documentation
tests/          Qt Test suite (placeholder)
```

---

## Documentation

- [Product Scope](docs/product-scope.md)
- [Architecture](docs/architecture.md)
- [GUI Layout](docs/gui-layout.md)
- [Debug Logging](docs/debug-logging.md)
- [Development Workflow](docs/development-workflow.md)
- [Project Status](docs/project-status.md)
- [Architecture Decisions](docs/architecture-decisions.md)
- [Build — Linux](docs/build-linux.md)
- [Build — Windows](docs/build-windows.md)
- [Build — macOS ARM64](docs/macos-arm64-build.md)
- [macOS Deployment](docs/macos-deployment.md)
- [macOS Platform Services](docs/macos-platform-services.md)
- [macOS Audio and Video](docs/macos-audio-video.md)
- [macOS Signing and Notarization](docs/macos-signing-notarization.md)
- [Windows ↔ macOS Interoperability Test](docs/windows-macos-interoperability-test.md)
- [Windows Portable Bundle](docs/windows-portable-bundle.md)
- [Windows Deployment Dependencies](docs/windows-deployment-dependencies.md)
- [Windows Clean-Machine Test Procedure](docs/windows-clean-machine-test.md)
- [ETSI Compatibility Overview](docs/etsi-compatibility/overview.md)

---

## License

To be determined. Private repository.
