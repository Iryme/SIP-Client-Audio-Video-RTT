# Project Handoff 001

Snapshot date: 2026-06-22  
Active branch: `feature/project-handoff-001`  
Snapshot base: `0db1e98` (Task 10)  
Project: **SIP Client Audio Video RTT**

This is the first formal project handoff. It describes the repository as it exists at the snapshot base, including an important branch-integration gap for Tasks 2-5.

## 1. Project Overview

SIP Client Audio Video RTT is a generic, cross-platform SIP multimedia client for Windows and Linux. The application uses C++17, Qt 6, CMake, and an optional PJSIP/pjsua2 backend.

Primary features:

- SIP registration
- Audio calls
- Video calls
- Diagnostics
- Media device management
- SIP profile management

Future optional features:

- RFC 4103 real-time text (RTT)
- ETSI TS 103 698 LMPE
- ETSI TS 103 479 compatibility
- ETSI TS 103 480 interoperability testing

At this handoff, the desktop foundation, profile and credential management, media-device enumeration, and SIP engine lifecycle skeleton exist. Registration, calls, media streaming, RTT, and ETSI behavior do not.

## 2. Current Architecture

The application is layered so GUI code does not own protocol or platform implementation details.

### Implemented modules

| Module | Current implementation |
|---|---|
| Diagnostics | `Logger` is a Qt singleton with levels, categories, and an `entryAdded` signal. `DiagnosticsPanel` displays entries, toggles levels, clears, copies, and exports visible rows. Search, category filtering, redaction, retention, and debug-bundle creation are not present in the active lineage. |
| Settings | Header-only `AppSettings` wraps user-scope INI `QSettings` for window geometry, splitter state, log-level flags, and media-device IDs. The richer `ApplicationSettings` Task 5 implementation exists only on `feature/persistent-settings`. |
| SIP Profiles | `SipProfile` and singleton `SipProfileManager` implement validation, CRUD, active-profile selection, INI persistence, URI derivation, and credential helper calls. |
| Secure Credential Storage | `CredentialStore` uses a pluggable `ICredentialBackend`. Windows uses Windows Credential Manager through Advapi32; tests use `MemoryCredentialBackend`. Passwords are kept out of profiles, settings, and logs. No Linux secure backend exists. |
| Media Devices | `MediaDeviceManager` enumerates microphones, speakers, and cameras through `QtMediaDeviceBackend`. `MediaDeviceSelectionModel` persists selections and falls back to defaults. `MediaPanel` provides selectors and manual refresh. |
| SIP Engine Skeleton | `SipManager` owns initialization/shutdown. Default stub mode has no PJSIP dependency. With `ENABLE_PJSIP=ON` and a discovered installation, it creates, initializes, starts, and destroys a pjsua2 `Endpoint`. `SipAccount` and `SipCall` remain stubs. |
| GUI Layout | `MainWindow` composes a menu bar, fixed navigation rail, profile/contact sidebar, call controls, video placeholder, RTT/LMPE placeholder, information tabs, diagnostics panel, and status bar using stable splitters. |

### Dependency graph

```text
main
`-- Application
    |-- SipManager
    |   `-- PJSIP/pjsua2 Endpoint (ENABLE_PJSIP + PJSIP found)
    `-- MainWindow
        |-- NavRail
        |-- SidebarPanel
        |   |-- SipProfileEditorDialog
        |   `-- SipProfileManager
        |       |-- QSettings (SIPClientProfiles.ini)
        |       |-- CredentialStore
        |       |   `-- ICredentialBackend
        |       |       |-- WindowsCredentialBackend -> Advapi32
        |       |       `-- MemoryCredentialBackend (tests)
        |       `-- Logger
        |-- CallPanel
        |-- VideoPanel
        |-- RttPanel
        |-- MediaPanel
        |   `-- MediaDeviceSelectionModel
        |       |-- AppSettings -> QSettings
        |       `-- MediaDeviceManager
        |           |-- QtMediaDeviceBackend -> Qt Multimedia
        |           `-- Logger
        |-- DiagnosticsPanel -> Logger
        `-- AppStatusBar
```

### Branch lineage warning

The repository has no `main` branch; `origin/HEAD` points to `feature/project-skeleton`. Tasks 2-5 are separate descendants in one line, while Tasks 6-10 are another line starting from Task 1. Consequently, the active handoff lineage does **not** contain the dedicated Task 2-5 commits. In particular, it lacks the richer `DiagnosticsLogger`, `LogFilterModel`, and `ApplicationSettings` modules and their tests, even though simpler logger, diagnostics-panel, and settings code is present. Reconcile these branches before extending those areas.

## 3. Implemented Tasks

### Task 1 - Project Skeleton

- Purpose: establish the Qt 6/CMake/C++17 application, directory structure, dark desktop shell, documentation framework, and placeholder feature boundaries.
- Major files: `CMakeLists.txt`, `README.md`, `src/main.cpp`, `src/app/*`, `src/gui/*`, `resources/*`, `docs/*`.
- Test results: no automated suite was established for the initial skeleton; current GUI startup was not manually smoke-tested during this documentation-only handoff.

### Task 2 - GUI Layout Specification

- Purpose: define the permanent desktop arrangement, minimum sizes, splitter behavior, responsive rules, and visual hierarchy.
- Major files on task branch: `docs/gui-layout.md`, `docs/architecture-decisions.md`, `README.md`, `docs/project-status.md`.
- Test results: documentation-only task; no automated tests. The full Task 2 commit `f10cf6e` is not in the active lineage.

### Task 3 - Diagnostics Logger

- Purpose: add structured, thread-safe diagnostics with levels, categories, redaction, retention/export behavior, and tests.
- Major files on task branch: `src/diagnostics/DiagnosticsLogger.*`, `LogEntry.h`, `LogLevel.h`, `LogCategory.h`, `tests/diagnostics/DiagnosticsLoggerTests.cpp`, `docs/debug-logging.md`.
- Test results: a dedicated diagnostics suite was added on `feature/diagnostics-logger`; it was not runnable from the active handoff lineage. Active code instead uses the simpler `src/core/Logger.*`. The Task 3 commit `3c4bd54` is not in the active lineage.

### Task 4 - Diagnostics GUI Console

- Purpose: connect a filterable GUI log console to `DiagnosticsLogger` and provide level/category/text filtering and export controls.
- Major files on task branch: `src/diagnostics/LogFilterModel.*`, `src/gui/panels/DiagnosticsPanel.*`, `tests/gui/LogFilterModelTests.cpp`, `docs/debug-logging.md`.
- Test results: a dedicated filter-model suite was added on `feature/diagnostics-gui-console`; it was not runnable from the active handoff lineage. Current `DiagnosticsPanel` supports level toggles, table display, copy, clear, and visible-row export, but no separate filter model. The Task 4 commit `1bd6cac` is not in the active lineage.

### Task 5 - Persistent Settings

- Purpose: provide typed, persistent application settings and migration/test isolation conventions.
- Major files on task branch: `src/settings/ApplicationSettings.*`, `src/core/AppSettings.h`, `tests/test_application_settings.cpp`, `tests/test_smoke.cpp`, `docs/application-settings.md`.
- Test results: settings and smoke suites were added on `feature/persistent-settings`; they were not runnable from the active handoff lineage. Current code has a smaller header-only `AppSettings`. The Task 5 commit `9029b6c` is not in the active lineage.

### Task 6 - SIP Profile Manager

- Purpose: model non-secret SIP account data and provide validation, CRUD, active selection, URI derivation, persistence, and GUI profile placeholders.
- Major files: `src/sip/SipProfile.h`, `src/sip/SipProfileManager.*`, `src/gui/panels/SidebarPanel.*`, `tests/test_sip_profile_manager.cpp`, `docs/sip-profiles.md`.
- Test results: `test_sip_profile_manager` passed under CTest at handoff (10 functional test methods; suite passed).

### Task 7 - Secure Credential Storage

- Purpose: store authentication passwords outside INI files behind a replaceable secure-backend interface.
- Major files: `src/security/ICredentialBackend.h`, `CredentialStore.*`, `WindowsCredentialBackend.*`, `MemoryCredentialBackend.*`, `tests/test_credential_store.cpp`, `docs/secure-credential-storage.md`.
- Test results: `test_credential_store` passed under CTest at handoff (10 functional test methods; suite passed).

### Task 8 - SIP Profile Editor

- Purpose: provide add/edit/delete profile workflows, validation, active-profile updates, password handling, and destructive-action confirmation.
- Major files: `src/gui/dialogs/SipProfileEditorDialog.*`, `src/gui/panels/SidebarPanel.*`, `tests/test_sip_profile_editor.cpp`, `docs/sip-profiles.md`, `docs/gui-layout.md`.
- Test results: `test_sip_profile_editor` passed under CTest at handoff (8 functional test methods; suite passed).

### Task 9 - Media Device Enumeration

- Purpose: enumerate audio/video devices, separate the Qt backend from selection logic, persist device choices, and fall back safely when a device disappears.
- Major files: `src/media/*`, `src/gui/panels/MediaPanel.*`, `tests/test_media_device.cpp`, `docs/media-audio-video.md`.
- Test results: `test_media_device` passed under CTest at handoff (4 functional test methods; suite passed).

### Task 10 - PJSIP Build Integration Skeleton

- Purpose: make PJSIP optional, discover it through CMake, and establish `SipManager` endpoint lifecycle while preserving a dependency-free stub build.
- Major files: `cmake/FindPJSIP.cmake`, `CMakeLists.txt`, `src/sip/SipManager.*`, `SipAccount.*`, `SipCall.*`, `tests/test_sip_manager.cpp`, `docs/build-windows.md`, `docs/build-linux.md`.
- Test results: `test_sip_manager` passed in stub mode under CTest at handoff (4 functional test methods; suite passed). A PJSIP-enabled build was not verified.

## 4. Current GUI Status

| Area | Implemented | Placeholder or limitation |
|---|---|---|
| Menu | File, View, Contacts, Calls, Messaging, Tools, and Help menus exist; Exit and Full Screen are connected. | Most other actions are unconnected placeholders. |
| Navigation rail | Fixed 64 px rail with Accounts, Contacts, Calls, Messages, History, Dialpad, and Settings buttons; emits `pageRequested`. | No icons and no page controller is connected. |
| Sidebar | Active profile selector, account card, profile add/edit/delete, profile editor integration, contact search field, and contact list. | Registration always reads Unregistered; contacts are hardcoded and search is not connected. |
| Diagnostics panel | Receives logger signals; displays time, level, category, message, and payload; level toggles, clear, copy, and text export work. | Search field is not connected, category filtering is absent, newly hidden levels are dropped rather than model-filtered, Bundle is informational only. |
| Media panel | Microphone, speaker, and camera selectors; persistence, fallback, and manual Refresh. | No capture/rendering, level meters, PJSIP binding, or automatic hot-plug refresh. |
| Status bar | Shows connection, account, transport, IP, jitter, loss, RTT, and SIP backend slots. SIP backend name/initialization state is updated. | All other values are static placeholders and do not reflect registration or calls. |
| Call/video/RTT | Call controls and stable panels are laid out. | Calls, remote/local video, RTT, LMPE, Call Info, and Statistics are placeholders; Share and Record are disabled. |

## 5. Standards Status

| Standard/component | Status | Exact implementation |
|---|---|---|
| RFC 4103 RTT | **NOT STARTED** | UI placeholder and planning documentation only; no T.140 buffering, RTP payload handling, negotiation, redundancy, or RTT session engine. |
| ETSI TS 103 698 | **NOT STARTED** | LMPE placeholder tab and documentation only; no schema, message generation/parsing, transport, or emergency workflow. |
| ETSI TS 103 479 | **NOT STARTED** | Compatibility mapping documentation only; no executable compatibility behavior. |
| ETSI TS 103 480 | **NOT STARTED** | Interoperability test-plan documentation only; no test harness or test execution. |
| PJSIP | **PARTIAL** | Optional discovery/import target, `HAVE_PJSIP` compile guard, endpoint create/init/start/destroy lifecycle, stub fallback, logging, and backend status. No transport creation, accounts, registration, callbacks, calls, codecs, media binding, SDP inspection, or SIP tracing. |

## 6. Build Instructions

Prerequisites: CMake 3.16+, Qt 6.4+ with Core, Gui, Widgets, Multimedia, and MultimediaWidgets, plus a C++17 compiler. Add Qt to `CMAKE_PREFIX_PATH` if CMake cannot find it.

### Windows

From a Visual Studio developer shell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\SIPClient.exe
```

Ninja can be used instead when it and a configured MSVC environment are available.

### Linux

Ubuntu prerequisites:

```bash
sudo apt install cmake ninja-build git qt6-base-dev qt6-multimedia-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/SIPClient
```

Fedora package names are `qt6-qtbase-devel` and `qt6-qtmultimedia-devel`.

### Stub mode

Stub mode is the default and requires no PJSIP installation:

```bash
cmake -S . -B build -DENABLE_PJSIP=OFF -DBUILD_TESTS=ON
cmake --build build --parallel
```

`SipManager::initialize()` succeeds, `backendName()` returns `Stub SIP backend`, and registration/calls remain unavailable.

### ENABLE_PJSIP mode

Build PJSIP/pjsua2 separately, then point CMake at its installation prefix:

```bash
cmake -S . -B build-pjsip \
  -DENABLE_PJSIP=ON \
  -DPJSIP_DIR=/path/to/pjsip/install \
  -DBUILD_TESTS=ON
cmake --build build-pjsip --parallel
```

On Windows use an equivalent path such as `-DPJSIP_DIR=C:/pjsip/install`. `FindPJSIP.cmake` also checks the `PJSIP_DIR` environment variable, pkg-config, and common system paths. If PJSIP is not found, configuration warns and falls back to stub mode even when the option is ON. PJSIP-enabled mode was not verified at this handoff.

## 7. Test Status

Verification on 2026-06-22 used the already configured Windows `build` tree in stub mode:

```text
test_sip_profile_manager   PASS (10 functional test methods)
test_credential_store      PASS (10 functional test methods)
test_sip_profile_editor    PASS (8 functional test methods)
test_media_device          PASS (4 functional test methods)
test_sip_manager           PASS (4 functional test methods)
------------------------------------------------------------
CTest suites               5/5 passed, 0 failed
Functional test methods    36 represented
```

CTest completed in 0.54 seconds. A rebuild was attempted first but could not run because this pre-existing build tree uses NMake and `nmake` was not available in the current shell. The existing test executables all passed. No fresh compile, manual GUI smoke test, Linux test, PJSIP-enabled test, or Tasks 3-5 branch test was completed in this handoff.

## 8. Active Modules

| Requested module name | Active status |
|---|---|
| DiagnosticsLogger | Not present by that name in the active lineage; `src/core/Logger.*` is the active, simpler equivalent. Full `DiagnosticsLogger` exists on the unmerged Task 3 branch. |
| LogFilterModel | Not present in the active lineage. It exists on the unmerged Task 4 branch. |
| ApplicationSettings | Not present by that name in the active lineage; header-only `src/core/AppSettings.h` is active. Full `ApplicationSettings` exists on the unmerged Task 5 branch. |
| SipProfileManager | Active: validation, CRUD, active selection, persistence, credential helpers. |
| CredentialStore | Active: backend abstraction, Windows secure backend, in-memory test backend. |
| MediaDeviceManager | Active: Qt-backed microphone/speaker/camera enumeration and lookup. |
| MediaDeviceSelectionModel | Active: selection persistence and default fallback. |
| SipManager | Active lifecycle skeleton: stub and optional PJSIP endpoint modes. |

## 9. Open Issues

- No `main` branch exists; `origin/HEAD` points to `feature/project-skeleton` despite workflow documentation naming `main`.
- Tasks 2-5 are not ancestors of Tasks 6-10; their richer implementations and tests require reconciliation, not blind merging.
- No SIP registration, transport/account creation, registration callback dispatch, or retry behavior.
- `SipAccount` and `SipCall` are stubs; no audio or video calls.
- PJSIP callbacks are not marshalled to the Qt main thread.
- No real media capture, playback, rendering, codec selection, or PJSIP device binding.
- Media hot-plug signals are not forwarded; Refresh is manual.
- Audio level meters, video preview, Call Info, Statistics, RTT, and LMPE are placeholders.
- Diagnostics search is not wired; category filtering, bounded retention, robust redaction, SIP ladder view, and bundle export are absent in the active lineage.
- Linux secure credential storage is not implemented; Windows credentials use local-machine persistence.
- Contacts and contact search are placeholders.
- Most menu and navigation actions are not connected.
- Status and account registration indicators are mostly static.
- Custom SIP headers are a profile-editor placeholder; no inline validation highlighting.
- RFC 4103 and all named ETSI standards remain documentation-only.
- No fresh Windows build was completed in this handoff because NMake was unavailable; no Linux CI result or PJSIP-enabled result exists.
- README and some older status language are stale relative to Tasks 6-10.

## 10. Recommended Next 10 Tasks

The requested roadmap numbering overlaps this handoff task number. This document records Project Handoff 001 as the current Task 11; the following labels are the requested feature-roadmap identifiers.

| Task | Recommended work |
|---|---|
| 11 | **SIP Register / Unregister** - create and remove a PJSIP account from the active profile and credential, while preserving stub builds. |
| 12 | **Registration State Machine** - normalize states, callbacks, errors, retries/backoff, and Qt-thread delivery. |
| 13 | **Account Status UI** - bind registration state/error/expiry to the sidebar and status bar. |
| 14 | **Audio Device Binding** - map persisted Qt device choices to PJSIP capture/playback devices and handle fallback. |
| 15 | **Audio Call Control** - outgoing/incoming audio call actions, answer/reject/hangup, mute, hold, and basic media connection. |
| 16 | **Call State Machine** - define call lifecycle, legal transitions, error mapping, and Qt signals. |
| 17 | **Call History** - persist non-secret call metadata and expose it in the GUI. |
| 18 | **SIP Ladder View** - capture sanitized SIP messages and render an ordered transaction/dialog view. |
| 19 | **Debug Bundle Export** - package redacted logs, environment/build details, and optional SIP traces without secrets. |
| 20 | **RTT RFC4103 Skeleton** - add T.140/session abstractions, SDP capability scaffolding, and tests without claiming interoperability. |

Before Task 11 feature work, resolve the missing stable branch and Tasks 2-5 lineage. Registration should use the richer diagnostics/settings modules or explicitly choose and document the simpler active variants.

## 11. Prompt for New Chat

```text
Continue development of "SIP Client Audio Video RTT" in the repository
https://github.com/Iryme/SIP-Client-Audio-Video-RTT.

Goal: build a generic cross-platform Windows/Linux SIP multimedia desktop client with SIP registration, audio/video calls, diagnostics, media-device management, and SIP profiles. Future optional work includes RFC 4103 RTT and ETSI TS 103 698/103 479/103 480 support.

Architecture: C++17 + Qt 6 + CMake. MainWindow contains the menu, NavRail, SidebarPanel, CallPanel, VideoPanel, RttPanel, MediaPanel, DiagnosticsPanel, and AppStatusBar. SipProfileManager persists non-secret profiles. CredentialStore uses Windows Credential Manager with an injectable memory test backend. MediaDeviceManager/MediaDeviceSelectionModel enumerate and persist Qt Multimedia devices. SipManager provides a default stub backend and an optional PJSIP/pjsua2 Endpoint lifecycle through ENABLE_PJSIP. SipAccount and SipCall are still stubs.

Completed task branches:
1 Project Skeleton
2 GUI Layout Specification
3 Diagnostics Logger
4 Diagnostics GUI Console
5 Persistent Settings
6 SIP Profile Manager
7 Secure Credential Storage
8 SIP Profile Editor
9 Media Device Enumeration
10 PJSIP Build Integration Skeleton
11 Project Handoff 001

Active branch state: feature/project-handoff-001 is based on Task 10 commit 0db1e98. There is no main branch; origin/HEAD points to feature/project-skeleton. Tasks 2-5 were developed on a separate lineage and are not ancestors of Tasks 6-10. Therefore the active branch uses simpler Logger, DiagnosticsPanel, and AppSettings implementations; the richer DiagnosticsLogger, LogFilterModel, and ApplicationSettings plus their tests exist only on the unmerged Task 3-5 branches. Treat this as the first issue to reconcile. At handoff, the five active CTest suites passed (36 functional test methods represented), but a fresh build was blocked because NMake was unavailable in the shell. PJSIP-enabled and Linux builds were not tested.

Read docs/project-handoff/handoff-001.md and docs/project-status.md before changing code. Preserve stub-mode compilation and never store or log credentials.

Next recommended feature task: Task 11 - SIP Register / Unregister. First establish/reconcile the stable branch lineage, then wire the active SipProfileManager and CredentialStore into a PJSIP SipAccount, implement register/unregister and Qt-thread-safe callbacks, add tests, and update sidebar/status indicators. Do not implement calls or RTT in that task.
```
