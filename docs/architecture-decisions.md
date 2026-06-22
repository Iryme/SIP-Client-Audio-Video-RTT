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

## ADR-008 — Fixed docked layout with 1024×768 minimum support

**Decision:** The application window uses a fixed docked layout defined in `docs/gui-layout.md`. All panels are permanently docked. No core panel may become a floating window. The layout must be fully usable at 1024×768.

**Rationale:**
- A softphone UI must be stable and predictable. Users in audio/video calls cannot afford layout surprises.
- Floating docks introduce edge cases: off-screen positions, Z-order conflicts, window manager interactions, and restore problems across monitor configurations and multi-machine setups.
- 1024×768 is the minimum deployed resolution in enterprise and accessibility contexts.
- Fixed docking simplifies layout persistence (only splitter states need saving, not panel positions).

**Constraints enforced:**

| Rule | Mechanism |
|---|---|
| No floating windows | No `QDockWidget` in floating mode; all panels docked or tabbed |
| Panel minimum sizes | `setMinimumWidth()` / `setMinimumHeight()` on every panel |
| No accidental collapse | `QSplitter::setChildrenCollapsible(false)` on all splitters |
| Nav rail always visible | `setFixedWidth(64)` — outside splitter, never hidden |
| Call controls always visible | `CallPanel` fixed height inside center widget, above video |
| Diagnostics always docked | Bottom section of vertical splitter, minimum 160 px |
| Status bar fixed | `setFixedHeight(30)`, `setSizeGripEnabled(false)` |
| Layout persistence | `QSplitter::saveState()` / `restoreState()` via `AppSettings` |

**Specific dimension contract (see `docs/gui-layout.md` for full table):**

| Panel | Default | Minimum |
|---|---|---|
| Menu bar height | 40 px | 40 px (fixed) |
| Nav rail width | 64 px | 64 px (fixed) |
| Sidebar width | 260 px | 220 px |
| Center area width | ~700 px | 420 px |
| Right RTT/LMPE width | 380 px | 320 px (or collapse to tab) |
| Diagnostics height | 250 px | 160 px |
| Status bar height | 30 px | 30 px (fixed) |

**Consequences:**
- Users cannot detach or freely rearrange panels.
- Panel visibility toggling (View menu) hides panels by setting minimum/maximum size to zero — panels do not float.
- The RTT/LMPE panel at minimum window width collapses into a tab adjacent to Call Info tabs; this is the only permitted layout adaptation at minimum resolution.
- Future feature panels (e.g. ETSI status, keypad overlay) must also be docked, not floating.

**Reference:** `docs/gui-layout.md` is the authoritative specification for all dimensions and region contents.
