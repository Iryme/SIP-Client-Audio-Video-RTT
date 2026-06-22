# Architecture Decisions

## ADR-001 — Qt 6 C++ as GUI framework

**Decision:** Use Qt 6 with C++17.

**Rationale:** Cross-platform (Windows/Linux), mature multimedia APIs, QSplitter for stable layouts, QStyleSheet for theming, Qt Multimedia for audio/video surface integration. Native look and feel without web engine overhead.

**Consequences:** Requires Qt 6.4+ on target machines. License must be tracked (LGPL or commercial).

---

## ADR-002 — PJSIP / pjsua2 as SIP stack

**Decision:** Use PJSIP with the pjsua2 C++ API.

**Rationale:** Mature, well-tested, supports SIP/SDP/RTP/SRTP/ICE/STUN/TURN. pjsua2 provides a higher-level C++ wrapper that integrates cleanly with Qt via callbacks posted to the main thread.

**Status:** Planned — not yet integrated.

**Consequences:** PJSIP must be built separately or provided as a system library. CMake find module required.

---

## ADR-003 — Splitter-based fixed layout

**Decision:** Use `QSplitter` with `setChildrenCollapsible(false)` and enforced minimum sizes. No floating docks for core panels.

**Rationale:** Guarantees stable layout at minimum resolution (1024×768). Eliminates layout jumping during state changes. Splitter positions are persisted via `QSettings`.

**Consequences:** Panels cannot be detached or rearranged freely. This is intentional for a professional softphone.

---

## ADR-004 — Logger as singleton with Qt signals

**Decision:** `Logger` is a `QObject` singleton. It emits `entryAdded(LogEntry)` which `DiagnosticsPanel` connects to via Qt signal/slot.

**Rationale:** Decouples log producers from the UI. Thread-safe delivery via queued connections when logging from background threads.

**Consequences:** Logger must be initialized before the main window. All connections from background threads must use `Qt::QueuedConnection`.

---

## ADR-005 — ETSI modules are strictly optional

**Decision:** ETSI TS 103 479, 103 480, 103 698 are compiled only when `BUILD_ETSI=ON` CMake option is set.

**Rationale:** The base product is a generic SIP client. Emergency compliance features must not affect build or runtime of the standard client.

**Consequences:** ETSI code lives in `src/etsi/`, loaded dynamically or compiled in only when requested. No ETSI headers in core modules.

---

## ADR-006 — Passwords never stored in plain text

**Decision:** SIP account passwords will use the OS keychain (Windows Credential Manager, Linux libsecret/kwallet). The INI `QSettings` file must never contain passwords.

**Status:** Planned — implementation deferred to SIP profile task.

---

## ADR-007 — No blocking calls on the GUI thread

**Decision:** All SIP operations, media I/O, and file I/O run on background threads. Results are posted to the main thread via `QMetaObject::invokeMethod` or queued signals.

**Rationale:** Keeps the GUI responsive at all times. PJSIP callbacks run on PJSIP threads — they must never touch Qt widgets directly.

---

## ADR-009 — DiagnosticsLogger as canonical logging foundation

**Decision:** All application modules (SIP, Media, RTT, LMPE, ETSI, App) log through
`DiagnosticsLogger` in `src/diagnostics/`. The GUI signal adapter (`src/core/Logger`) remains
as a thin wrapper that re-emits Qt signals for `DiagnosticsPanel`. `DiagnosticsLogger` is
not a `QObject` and has no Qt event loop dependency.

**Rationale:**
- A non-QObject singleton can be called from PJSIP callbacks, media threads, and any
  background thread without marshalling to the main thread first.
- Thread safety is provided by an internal `QMutex`, so callers need no external synchronization.
- Automatic redaction (passwords, tokens, keys) is enforced at the store boundary — no module
  can accidentally persist sensitive data by forgetting to sanitize before logging.
- In-memory buffer (10 000 entries, configurable) prevents unbounded memory growth during
  long calls with DEBUG or RAW enabled.
- Plain text and JSON-ready export formats enable both human-readable log files and
  structured analysis pipelines.

**Redacted key patterns:** `password`, `passwd`, `secret`, `token`, `authorization`,
`private key`, `auth` — matched case-insensitively before `=` or `:`.

**Consequences:**
- `src/core/Logger` must eventually delegate to `DiagnosticsLogger` rather than maintaining
  a separate enabled-level map. This unification is deferred to just before SIP integration.
- RAW level requires explicit `setLevelEnabled(LogLevel::Raw, true)` — no code path enables
  it automatically.
- Unit tests in `tests/diagnostics/DiagnosticsLoggerTests.cpp` cover: default levels,
  enable/disable logic, RAW off by default, category filtering, clear, redaction (7 patterns),
  and export formats.
- Build tests with: `cmake -DBUILD_TESTS=ON .. && cmake --build . && ctest --output-on-failure`
