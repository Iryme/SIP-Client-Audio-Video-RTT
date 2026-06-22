# SIP Client — Audio / Video / RTT

A generic, cross-platform SIP multimedia desktop client with a professional dark GUI.

**Platforms:** Windows 10/11, Linux (Ubuntu 22.04+, Fedora 38+)
**GUI:** Qt 6 C++
**Build:** CMake 3.16+
**Status:** Foundation skeleton — GUI layout only, no SIP networking yet

---

## Features (planned)

| Feature | Status |
|---|---|
| Qt 6 GUI skeleton | IMPLEMENTED |
| Dark professional theme | IMPLEMENTED |
| SIP profile management | NOT STARTED |
| SIP registration | NOT STARTED |
| Audio calls | NOT STARTED |
| Video calls | NOT STARTED |
| RFC 4103 RTT | NOT STARTED |
| LMPE messaging | NOT STARTED |
| ETSI TS 103 479 | NOT STARTED |
| ETSI TS 103 480 | NOT STARTED |
| ETSI TS 103 698 | NOT STARTED |

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

See [docs/build-linux.md](docs/build-linux.md) and [docs/build-windows.md](docs/build-windows.md) for detailed instructions.

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
- [ETSI Compatibility Overview](docs/etsi-compatibility/overview.md)

---

## License

To be determined. Private repository.
