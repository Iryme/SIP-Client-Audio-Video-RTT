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

## ADR-008 — QSettings INI for application preferences

**Decision:** Use `QSettings` with `QSettings::IniFormat` and `QSettings::UserScope` (org: `SIPClient`, app: `SIPClient`) for all non-secret application preferences.

**Rationale:** QSettings is part of Qt Core — no extra dependency. INI format is human-readable and debuggable. User scope ensures settings are per-user without requiring admin rights.

**Consequences:** All future modules add typed accessors to `ApplicationSettings`. Tests use a separate org/app name to avoid polluting real user settings.

---

## ADR-009 — Credential storage separated from QSettings

**Decision:** SIP account passwords and authentication secrets are never stored in `ApplicationSettings` or any INI file. They are deferred to the OS credential store: Windows Credential Manager on Windows, libsecret/KWallet on Linux.

**Rationale:** INI files are plain text readable by any process with filesystem access to the user profile. OS keystores provide encryption tied to the user session.

**Status:** Deferred — credential storage implementation is part of the SIP profile task.

**Consequences:** `ApplicationSettings` has no `setPassword()` or `setAuthSecret()` methods — by design. `SipProfileManager` has no password fields — by design.

---

## ADR-010 — SIP profiles stored in a separate QSettings file

**Decision:** SIP profile data (non-secret) is persisted in a dedicated INI file (`SIPClientProfiles.ini`) via `QSettings(IniFormat, UserScope, "SIPClient", "SIPClientProfiles")`, separate from general application preferences (`SIPClient.ini`).

**Rationale:** Profile data has a different lifecycle than UI preferences. Keeping them in separate files means: (1) UI preferences can be reset without losing account configuration; (2) profiles can eventually be exported/imported independently; (3) the main settings file stays small and readable.

**Consequences:**
- `SipProfileManager` owns its own `QSettings` instance — it does not depend on `ApplicationSettings`.
- Tests use `IrymeTest/SIPClientTest_Profiles` as the isolated org/app pair.
- Key layout documented in `docs/sip-profiles.md`.

---

## ADR-011 — CredentialStore abstracts the OS keychain behind a pluggable backend

**Decision:** Credential storage is encapsulated in `CredentialStore` (`src/security/`), which owns an `ICredentialBackend` implementation. The production backend is `WindowsCredentialBackend` (Windows Credential Manager). Tests inject `MemoryCredentialBackend` via `setBackend()`. No production code directly calls WinCred APIs outside `WindowsCredentialBackend`.

**Rationale:**
- Isolates platform-specific credential APIs to one file, making future Linux/macOS backends easy to add.
- Enables unit testing without touching the real OS keychain (`MemoryCredentialBackend` is deterministic and ephemeral).
- Provides a single choke point for logging: only `CredentialStore` logs credential events, and it never logs password values.
- `SipProfileManager` credential helpers (`setProfilePassword`, `hasProfilePassword`, `removeProfilePassword`) delegate to `CredentialStore` so that call sites never need to know the underlying backend.

**Consequences:**
- `WindowsCredentialBackend.cpp` is compiled only on Windows (`if(WIN32)` in CMakeLists).
- On non-Windows platforms `CredentialStore` has no backend (`m_backend = nullptr`) and all operations fail with an error log — there is **no silent insecure fallback**.
- Tests use `IrymeTest/SIPClientTest_Credentials` QSettings namespace (SipProfileManager part) and a `MemoryCredentialBackend` injected before each test case.
- Credential key format: `SIPClient/sip/<profileId>/<username>` (documented in `docs/secure-credential-storage.md`).
