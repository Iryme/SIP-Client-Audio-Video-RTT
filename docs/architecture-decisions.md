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

## ADR-008 — LogFilterModel isolates filter logic from DiagnosticsPanel (Task 4)

**Decision:** Filter, category, and search logic for the Diagnostics GUI Console lives in
`src/diagnostics/LogFilterModel`, a `QObject` with no GUI dependency, rather than inline
in `DiagnosticsPanel`.

**Rationale:**
- `DiagnosticsPanel` is a GUI widget — it cannot be instantiated in a headless test
  (`QTEST_GUILESS_MAIN`). Placing filter logic there makes it untestable without a display.
- `LogFilterModel` is a plain `QObject` with no widget dependencies, so tests run with
  `QTEST_GUILESS_MAIN` in CI without a display server.
- The model emits `filterChanged()` when any filter setter changes a value (no-op if same value),
  so `DiagnosticsPanel` simply calls `rebuildTable()` on that signal — separation of concerns.
- `addEntry()` / `clear()` / `filteredEntries()` provide a clean boundary: the panel feeds
  entries in and queries filtered views out.

**Consequences:**
- `DiagnosticsPanel` owns `LogFilterModel` as a child QObject; entry storage and Logger's
  own buffer are independent (the panel's copy is used for re-filtering on demand).
- 23 unit tests in `tests/gui/LogFilterModelTests.cpp` cover: default states, level filter,
  RAW visibility, category filter, search text, combined filters, and signal emission.

---

## ADR-009 — DiagnosticsLogger upgraded to QObject with entryAdded signal (Task 4)

**Decision:** `DiagnosticsLogger` was changed from a plain C++ singleton to a `QObject`
singleton with an `entryAdded(LogEntry)` signal. `DiagnosticsPanel` connects to it directly.

**Rationale:**
- Eliminates the intermediate `src/core/Logger` class as the GUI signal source. The panel
  now consumes `DiagnosticsLogger` directly, satisfying the requirement that GUI must consume
  the DiagnosticsLogger API.
- Signal is emitted **after** the mutex is released so there is no deadlock risk if a slot
  calls back into `DiagnosticsLogger::entries()`.
- Background threads log via `DiagnosticsLogger::log()`, which will emit the signal on the
  calling thread. Connecting from `DiagnosticsPanel` (main thread) with the default
  `Qt::AutoConnection` ensures the slot runs on the main thread via queued delivery.

**Consequences:**
- `src/core/Logger` is now legacy and is compiled but not connected to `DiagnosticsPanel`.
  It should be removed or unified in a future refactor before SIP integration.
- RAW is still always `false` on startup — the constructor sets it to `false` and the panel
  enforces a confirmation dialog before enabling it.

---

## ADR-010 — DiagnosticsLogger as canonical logging foundation (Task 3)

**Decision:** All application modules log through `DiagnosticsLogger` in `src/diagnostics/`.
In Task 4, DiagnosticsLogger was upgraded to a QObject so DiagnosticsPanel connects to it
directly. `src/core/Logger` is retained but is no longer the GUI signal source (see ADR-009).

**Rationale:**
- DiagnosticsLogger can be called from PJSIP callbacks, media threads, and any
  background thread; the QMutex ensures thread safety.
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
