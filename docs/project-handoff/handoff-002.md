# Project Handoff 002

Snapshot date: 2026-06-23  
Active branch: `feature/project-handoff-002`  
Snapshot base: `0f94065` (Task 20)  
Project: **SIP Client Audio Video RTT**

This is the second formal project handoff. It covers Tasks 12-20 (SIP registration through SIP diagnostics). Tasks 1-11 are documented in `handoff-001.md`; this document provides a complete continuation prompt independently of it.

> **WARNING — Real PJSIP validation is outstanding.** Every feature in Tasks 12-20 that exercises the PJSIP code path was implemented and compiled with `ENABLE_PJSIP=OFF` (stub mode). No feature has been tested against a live PJSIP installation, a real SIP registrar, or a real SIP client peer. Real PJSIP registration, calls, and media streaming still need end-to-end validation on a machine with PJSIP installed and a SIP server/client pair.

---

## 1. Project Overview

SIP Client Audio Video RTT is a generic cross-platform SIP multimedia desktop client targeting Windows and Linux. It is written in C++17 with Qt 6, CMake, and an optional PJSIP/pjsua2 backend.

Primary planned features:

- SIP registration (manual, retry/backoff, expiry refresh)
- Outgoing and incoming audio/video calls
- Diagnostics panel with SIP ladder view and export
- Media device enumeration and selection
- SIP profile management
- Secure credential storage (OS keychain)

Future optional features:

- RFC 4103 real-time text (RTT) — not started
- ETSI TS 103 698 LMPE — not started
- ETSI TS 103 479 compatibility — not started
- ETSI TS 103 480 interoperability testing — not started

---

## 2. Architecture Principles

These constraints must be respected by all future tasks.

| Constraint | Rule |
|---|---|
| Passwords | **Never** in `QSettings`, `SipProfile`, logs, or signal payloads. OS keychain only (`CredentialStore`). |
| Platform guards | Use `#ifdef _WIN32` (compiler native) for guards that appear before Qt headers. Never `#ifdef Q_OS_WIN` there. |
| PJSIP isolation | `HAVE_PJSIP` guards all `pjsua2.hpp` code. Only `SipManager.cpp`, `SipAccount.cpp`, and `SipCall.cpp` may include pjsua2 headers. All other modules (`SipTraceLogger`, `AudioMediaManager`, `VideoMediaManager`, …) are Qt-only. |
| PJSIP default | `ENABLE_PJSIP=OFF` is the default. Pass `-DENABLE_PJSIP=ON -DPJSIP_DIR=<path>` to enable. |
| Qt thread safety | All PJSIP callbacks must dispatch to the Qt main thread via `QMetaObject::invokeMethod` with `Qt::QueuedConnection`. |
| CredentialStore key | `SIPClient/sip/<profileId>/<username>` in Windows Credential Manager. |
| Test isolation | Each test file sets a unique `QCoreApplication::setOrganizationName` + `setApplicationName` to avoid polluting real `QSettings`. |
| RAW log level | Default `false`. No code enables it unconditionally. |

---

## 3. Current Branch and Commit State

| Branch | Last commit | Task |
|---|---|---|
| `feature/project-skeleton` | (origin/HEAD, base) | Task 1 |
| `feature/project-handoff-001` | `3270e50` | Task 11 |
| `feature/sip-registration` | `ec8c220` | Task 12 |
| `feature/registration-state-machine` | `9f7ceb0` | Task 13 |
| `feature/registration-retry-backoff` | `b060c16` | Task 14 |
| `feature/registration-expiry-refresh` | `26e25f3` | Task 15 |
| `feature/profile-switch-sequencing` | `9df2ee0` | Task 16 |
| `feature/call-state-machine` | `10b11a8` | Task 17 |
| `feature/audio-media` | `e779189` | Task 18 |
| `feature/video-media` | `109458d` | Task 19 |
| `feature/sip-diagnostics-ladder` | `0f94065` | Task 20 |
| **`feature/project-handoff-002`** | **this commit** | **Task 21** |

**Branch lineage note:** There is no `main` branch; `origin/HEAD` points to `feature/project-skeleton`. Each feature branch was created from the previous one; this is a linear chain. Tasks 2-5 remain on a separate, unmerged lineage (see handoff-001 for details). The active code uses the simpler `Logger`, `DiagnosticsPanel`, and `AppSettings` variants; the Tasks 3-5 richer implementations are not merged.

---

## 4. Summary of All Completed Tasks

### Tasks 1-11 (covered in detail by handoff-001)

| Task | Description |
|---|---|
| 1 | Project skeleton — Qt 6/CMake/C++17 desktop shell, layout, dark theme, docs |
| 2 | GUI layout specification — document-only |
| 3 | Diagnostics logger — richer logger (separate lineage, not merged) |
| 4 | Diagnostics GUI console — filter model (separate lineage, not merged) |
| 5 | Persistent settings — `ApplicationSettings` (separate lineage, not merged) |
| 6 | SIP profile model — `SipProfile`, `SipProfileManager`, `SidebarPanel` |
| 7 | Secure credential storage — `CredentialStore`, `WindowsCredentialBackend`, `MemoryCredentialBackend` |
| 8 | SIP profile editor dialog — `SipProfileEditorDialog`, Add/Edit/Delete with password integration |
| 9 | Media device enumeration — `MediaDeviceManager`, `MediaDeviceSelectionModel`, `MediaPanel` |
| 10 | PJSIP build integration skeleton — `FindPJSIP.cmake`, `ENABLE_PJSIP`, `SipManager` endpoint lifecycle |
| 11 | Project handoff 001 — formal project snapshot |

### Task 12 — SIP Register / Unregister

Branch: `feature/sip-registration` | Commit: `ec8c220`

`SipManager::registerActiveProfile()` looks up the active `SipProfile`, fetches the password from `CredentialStore`, creates (or reuses) a `SipAccount`, and calls `startRegistration()`. `unregisterActiveProfile()` calls `startUnregistration()`. All PJSIP callbacks (`onRegState`) dispatch to the Qt main thread via `invokeMethod`. The status bar and sidebar register/unregister buttons reflect the reported state. Tests cover stub mode, idempotent registration, and password guard.

### Task 13 — Registration State Machine

Branch: `feature/registration-state-machine` | Commit: `9f7ceb0`

`RegistrationStateMachine` enforces a five-state model: `Unregistered → Registering → Registered → Unregistering → RegistrationFailed`. `tryTransition()` enforces the transition table and logs every transition. A 30-second watchdog timer fires `RegistrationFailed` if `Registering` does not complete; it fires `Unregistered` if `Unregistering` times out. `reset()` bypasses the table for shutdown. `SipManager` drives the SM from callbacks. The Edit/Delete profile buttons are disabled in `Registering` and `Unregistering` states.

### Task 14 — Registration Retry / Backoff

Branch: `feature/registration-retry-backoff` | Commit: `b060c16`

`RegistrationRetryPolicy` (plain struct, not a QObject) defines `isRetryable(statusCode)` — retryable codes: 0 (timeout), 408, 5xx; non-retryable: 401, 403, 404, 423 — and `delayForAttempt(n)` using exponential backoff (`2s × 2^n`, cap 60 s). `SipManager` maintains a retry counter, schedules retries on `RegistrationFailed`, emits `retryScheduled(delayMs, attempt)`, and cancels the timer on `unregisterActiveProfile()` and `shutdown()`. A `setRetryPolicy()` injection method supports test override.

### Task 15 — Registration Expiry & Auto Re-REGISTER

Branch: `feature/registration-expiry-refresh` | Commit: `26e25f3`

`RegistrationRefreshConfig` (plain struct) defines `delayMsForExpiry(expiry)`: fires at `min(expiry × 0.8, expiry − 30 s)`, floor `expiry / 2`. An `overrideDelayMs` field allows test injection. `SipManager` schedules a refresh timer on successful registration, fires `onRefreshTimerFired()` → `m_account->refreshRegistration()`. Success resets retry counter and reschedules; failure transitions the SM to `RegistrationFailed` and lets retry logic take over. `registrationStatusText()` returns `"Registered (refreshing...)"` while a refresh is in flight.

### Task 16 — Profile Switch Sequencing

Branch: `feature/profile-switch-sequencing` | Commit: `9df2ee0`

`SipManager::switchActiveProfile(newId)` replaces the old `setActiveProfileId()` call in `SidebarPanel`. Fast path (state `Unregistered` or `RegistrationFailed`): immediate switch, no waiting. Slow path (any registering/registered/unregistering state): stores `m_pendingProfileId`, starts unregistration of the old account, waits for `Unregistered` state, then calls `completePendingSwitch()` → registers the new account. The sidebar profile combo is disabled during a pending switch; `profileSwitchStarted` / `profileSwitchCompleted` / `profileSwitchFailed` signals control it. `shutdown()` emits `profileSwitchFailed` and clears `m_pendingProfileId` if a switch is in flight.

### Task 17 — SIP Call State Machine

Branch: `feature/call-state-machine` | Commit: `10b11a8`

`CallStateMachine` enforces nine states: `Idle → OutgoingInit → Ringing → IncomingRinging → Connecting → Active → Held → Disconnecting → Failed`. Per-state display text is provided via `callStateDisplayText()`. A 30-second watchdog fires on `OutgoingInit` and `Disconnecting`. `SipCall` owns a `CallStateMachine`; it exposes `makeCall`, `answer`, `reject`, `hangup`, `hold`, `resume`. In stub mode a `postStubTransition()` helper posts state advances via `QMetaObject::invokeMethod`. `SipManager` is extended with `makeCall`, `answerCall`, `rejectCall`, `hangupCall`, `holdCall`, `resumeCall` — all forwarding to `m_activeCall`. `CallPanel` buttons are wired.

### Task 18 — Audio Media Integration

Branch: `feature/audio-media` | Commit: `e779189`

`AudioMediaManager` (Qt-only singleton) manages the audio side of the active call: `attachCall(SipCall*)` / `detachCall()` lifecycle driven by `SipManager`, `setMuted(bool)` forwarded to `SipCall::setMuted`, device selection (`setMicrophone` / `setSpeaker`) persisted via `MediaDeviceSelectionModel`, and `inputLevel` / `outputLevel` signal forwarding. The PJSIP bridge (guarded by `HAVE_PJSIP` in `SipCall.cpp`) wires `pj::AudioMedia` and a level timer; stub mode shows zero levels. `CallPanel` selectors, mute button, and level indicators are wired.

### Task 19 — Video Media Integration

Branch: `feature/video-media` | Commit: `109458d`

`VideoMediaManager` (Qt-only singleton) mirrors `AudioMediaManager` for video: `attachCall` / `detachCall`, `setVideoMuted(bool)` → `SipCall::setVideoMuted`, `setCamera` persisted via `MediaDeviceSelectionModel`, `isLocalVideoAvailable` / `isRemoteVideoAvailable` signals. The PJSIP bridge (guarded in `SipCall.cpp`) scaffolds `pj::VideoWindow` attachment; rendering to a native widget handle is deferred. `VideoPanel` shows mute/unmute controls, PiP overlay, and camera selector. Swap local/remote is wired as a UI toggle.

### Task 20 — SIP Diagnostics & SIP Ladder

Branch: `feature/sip-diagnostics-ladder` | Commit: `0f94065`

`SipMessageTrace` (POD struct, `Q_DECLARE_METATYPE`) carries: `direction`, `method`, `statusCode`, `statusText`, `fromUri`, `toUri`, `callId`, `cSeq`, `rawSip`. `SipTraceLogger` (Qt-only singleton) redacts `Authorization:` and `Proxy-Authorization:` header values to `[REDACTED]` before storing, emits `messageLogged(SipMessageTrace)`, and provides `exportToText()` / `exportToJson()` (JSON omits `rawSip` by design). `SipLadderWidget` is a custom `QWidget` with a `paintEvent` that draws two entity columns ("Local (UA)" / "Remote"), colored arrows, timestamps, and CSeq/Call-ID annotations. `DiagnosticsPanel` was restructured into a `QTabWidget` with Tab 0 "Log" (existing table unchanged) and Tab 1 "SIP Ladder". `SipManager` emits 11 synthetic trace points covering registration and call lifecycle.

---

## 5. Module Reference

| Module | Location | Status | Notes |
|---|---|---|---|
| `Logger` | `src/core/Logger` | Active | Singleton, levels, categories, `entryAdded` signal |
| `AppSettings` | `src/core/AppSettings.h` | Active | Header-only QSettings wrapper |
| `ApplicationSettings` | `src/settings/ApplicationSettings` | Active | Full persistent settings service |
| `SipProfile` | `src/sip/SipProfile.h` | Active | POD struct, no passwords, `effectiveSipUri()` |
| `SipProfileManager` | `src/sip/SipProfileManager` | Active | CRUD, validation, persistence, credential helpers |
| `CredentialStore` | `src/security/CredentialStore` | Active | Pluggable backend; Windows + Memory (test) |
| `WindowsCredentialBackend` | `src/security/WindowsCredentialBackend` | Active (Windows) | Advapi32 |
| `MemoryCredentialBackend` | `src/security/MemoryCredentialBackend` | Active (tests) | In-process only, not secure |
| `MediaDevice` | `src/media/MediaDevice.h` | Active | id, displayName, type, isDefault, isAvailable |
| `MediaDeviceManager` | `src/media/MediaDeviceManager` | Active | `QtMediaDeviceBackend`; pluggable `IMediaDeviceBackend` |
| `MediaDeviceSelectionModel` | `src/media/MediaDeviceSelectionModel` | Active | Persistence + default fallback |
| `AudioMediaManager` | `src/media/AudioMediaManager` | Active | Qt-only; PJSIP bridge in `SipCall.cpp` |
| `VideoMediaManager` | `src/media/VideoMediaManager` | Active | Qt-only; PJSIP bridge in `SipCall.cpp` |
| `SipManager` | `src/sip/SipManager` | Active | Registration lifecycle, call control, trace emission |
| `RegistrationStateMachine` | `src/sip/RegistrationStateMachine` | Active | 5-state SM, watchdog, transition guards |
| `RegistrationRetryPolicy` | `src/sip/RegistrationRetryPolicy` | Active | `isRetryable(code)`, exponential backoff |
| `RegistrationRefreshConfig` | `src/sip/RegistrationRefreshConfig` | Active | `delayMsForExpiry(expiry)`, `overrideDelayMs` |
| `SipAccount` | `src/sip/SipAccount` | Active | `pjsua2::Account` wrapper; queued callbacks |
| `CallStateMachine` | `src/sip/CallStateMachine` | Active | 9-state SM, watchdog, state display text |
| `SipCall` | `src/sip/SipCall` | Active | Owns `CallStateMachine`; stub + PJSIP paths |
| `SipMessageTrace` | `src/sip/SipMessageTrace.h` | Active | POD struct, `Q_DECLARE_METATYPE` |
| `SipTraceLogger` | `src/sip/SipTraceLogger` | Active | Singleton; redact, store, export, signal |
| `FindPJSIP.cmake` | `cmake/FindPJSIP.cmake` | Active | PJSIP discovery + `PJSIP::pjsua2` imported target |
| `SipLadderWidget` | `src/gui/SipLadderWidget` | Active | `paintEvent` ladder; lives in `DiagnosticsPanel` SIP tab |
| `SipProfileEditorDialog` | `src/gui/dialogs/SipProfileEditorDialog` | Active | Modal Add/Edit/Delete with `CredentialStore` integration |
| `DiagnosticsPanel` | `src/gui/panels/DiagnosticsPanel` | Active | QTabWidget: Log tab + SIP Ladder tab |
| `SidebarPanel` | `src/gui/panels/SidebarPanel` | Active | Profile selector, account card, register/unregister buttons |
| `MediaPanel` | `src/gui/panels/MediaPanel` | Active | Microphone/Speaker/Camera combos + Refresh |
| `CallPanel` | `src/gui/panels/CallPanel` | Active | Call controls, mute, level indicators, device selectors |
| `VideoPanel` | `src/gui/panels/VideoPanel` | Active | Video overlays, mute, camera selector, PiP placeholder |

---

## 6. Test Status

All tests were verified on 2026-06-23 in stub mode (`ENABLE_PJSIP=OFF`) on Windows using Qt 6.11.1 / MSVC 2022.

| Suite | Functional test methods | Result |
|---|---|---|
| `test_sip_profile_manager` | 12 | PASS |
| `test_credential_store` | 12 | PASS |
| `test_sip_profile_editor` | 10 | PASS |
| `test_media_device` | 6 | PASS |
| `test_registration_state_machine` | 28 | PASS |
| `test_sip_manager` | 15 | PASS |
| `test_registration_retry` | 14 | PASS |
| `test_registration_expiry` | 14 | PASS |
| `test_profile_switch` | 10 | PASS |
| `test_call_state_machine` | 10 | PASS |
| `test_audio_media` | 6 | PASS |
| `test_video_media` | 6 | PASS |
| `test_sip_trace` | 6 | PASS |
| `test_application_settings` | 8 | PASS |
| **Total** | **157** | **14/14 suites, 0 failures** |

Build: NMake generator, VS 2022 dev shell (`Launch-VsDevShell.ps1 -Arch amd64`), Qt at `F:\Programs\Qt\6.11.1\msvc2022_64`. Test executables require `-o results.xml,xml`; stdout is not captured by PowerShell redirect.

**CMakeLists.txt test-target rule:** any test target that compiles `SipManager.cpp` must include:

- `CALL_SOURCES` — `SipCall.cpp` + `CallStateMachine.cpp`
- `AUDIO_MEDIA_SOURCES` — `AudioMediaManager.cpp` + `VideoMediaManager.cpp` + `MediaDeviceManager.cpp` + `MediaDeviceSelectionModel.cpp` + `QtMediaDeviceBackend.cpp`
- `SIP_TRACE_SOURCES` — `SipTraceLogger.cpp`
- Link `Qt6::Multimedia`

---

## 7. What Is Not Yet Tested with Real PJSIP

The following behaviors exist as scaffolding code guarded by `HAVE_PJSIP` but have **not been executed** against a live PJSIP build:

| Behavior | Location | Risk |
|---|---|---|
| SIP REGISTER / 401 challenge / re-REGISTER | `SipAccount::startRegistration`, `SipManager::onAccountRegistrationStateChanged` | Credential flow, nonce handling, expiry extraction |
| Registration retry on transient failure | `SipManager::scheduleRetryIfEligible` | Retry counter, real 408/5xx codes from server |
| Registration expiry refresh | `SipManager::onRefreshTimerFired`, `SipAccount::refreshRegistration` | `setRegistration(true)` semantics, expiry extraction from `AccountInfo` |
| Outgoing INVITE / SDP negotiation | `SipCall::makeCall` (`pj::CallOpParam`) | Codec selection, ICE, SDP offer/answer |
| Incoming call delivery | `SipAccount::onIncomingCall`, `SipManager::onAccountIncomingCall` | Queued invokeMethod dispatch, call object ownership |
| Audio media connection | `SipCall::onCallMediaState` PJSIP path | `pj::AudDevManager`, `pjsua_conf_get_signal_level`, device-index mapping |
| Video media window | `SipCall::onCallMediaState` PJSIP path | `pj::VideoWindow`, native handle attachment |
| PJSIP account handle access | `SipManager::makeCall` → `m_account->pjAccountHandle()` | `SipAccount` currently has a TODO comment for exposing the `pj::Account&` accessor |
| Profile switch: unregister confirmation | `SipManager::completePendingSwitch` | Whether the server confirms UNREGISTER before replacement |
| Real SIP trace call-ids and URIs | `SipManager` emission points | Stub traces have synthetic values; real ones come from PJSIP callbacks |

---

## 8. Known Limitations

### Security

- `CredentialStore` is implemented for Windows only. Linux/macOS backends return failure; no silent fallback.
- `CRED_PERSIST_LOCAL_MACHINE` — Windows credentials are machine-bound, no roaming.

### Registration

- No automatic registration on startup or when a profile is selected for the first time.
- Network-change recovery not implemented.
- Watchdog timeout during profile switch proceeds with the new profile registration even if the old UNREGISTER was never acknowledged by the server; the old registration may persist until server-side expiry.
- Retry counter resets on exhaustion; no persistent backoff across app restarts.
- Expiry is extracted from PJSIP `AccountInfo.regExpiresSec`; if the server omits it, the configured default (300 s) is used.

### Calls

- One concurrent call supported; multi-party/conference deferred.
- Call duration timer is a UI placeholder; no elapsed-time counter.
- Incoming call identification (name resolution) not implemented; remote URI only.

### Audio / Video

- Audio bridge: PJSIP device-index mapping from Qt Multimedia string IDs is not implemented. Device selection persists but takes effect on the next call only.
- Audio level meters show 0 in stub mode.
- Real video rendering requires attaching a native window handle via `pj::VideoWindow` — not yet wired. `VideoPanel` shows a colour-tinted placeholder.
- Local camera preview before a call (without a SIP session) not started; `VideoPanel` PiP shows "Camera Off" until the call goes active.
- Camera hot-swap during a call: selection persists (takes effect on next call); device-index mapping not implemented.
- Audio/video bridge untested without live PJSIP installation.
- Hot-plug device detection (`QMediaDevices` signals) is not forwarded into `MediaDeviceManager`; user must press Refresh.

### SIP Diagnostics

- PJSIP path: synthetic traces are emitted in both stub and PJSIP builds but carry stub values (no real Call-IDs, no live URIs from callbacks). Byte-accurate raw SIP capture requires a PJSIP logging module hook — not implemented.
- JSON export omits `rawSip` by design (credential safety).
- Auto-scroll uses a queued `invokeMethod` which may occasionally lag one message.

### GUI

- Category filter and search in DiagnosticsPanel are UI-only; not wired to table filtering.
- Theme string is persisted but no theme-switch UI exists; always loads `dark_theme.qss`.
- Audio level meters in `MediaPanel` are static/inactive placeholders.
- Contact list is hardcoded; contacts and contact search are not implemented.
- Most menu and navigation actions remain disconnected.
- Custom SIP headers in the profile editor are a UI placeholder, not persisted.
- Debug bundle export is not implemented.

---

## 9. Build Instructions

### Prerequisites

- CMake 3.16+
- Qt 6.4+ with Core, Gui, Widgets, Multimedia, MultimediaWidgets
- C++17 compiler (MSVC 2022 on Windows; GCC/Clang on Linux)

### Windows (VS 2022 dev shell)

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -SkipAutomaticLocation
$env:CMAKE_PREFIX_PATH = "F:\Programs\Qt\6.11.1\msvc2022_64"
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run individual test executables with `-o results.xml,xml` for parseable XML output:

```powershell
.\build\tests\test_sip_manager.exe -o results.xml,xml
```

### Linux

```bash
sudo apt install cmake ninja-build qt6-base-dev qt6-multimedia-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Stub mode (no PJSIP required — default)

```bash
cmake -S . -B build -DENABLE_PJSIP=OFF -DBUILD_TESTS=ON
cmake --build build --parallel
```

`SipManager::initialize()` succeeds; `backendName()` returns `"Stub SIP backend"`. Registration, calls, and media streaming are simulated by internal state transitions.

### ENABLE_PJSIP mode

Build PJSIP/pjsua2 separately, then:

```bash
cmake -S . -B build-pjsip \
  -DENABLE_PJSIP=ON \
  -DPJSIP_DIR=/path/to/pjsip/install \
  -DBUILD_TESTS=ON
cmake --build build-pjsip --parallel
```

`FindPJSIP.cmake` also checks the `PJSIP_DIR` environment variable, pkg-config, and common system paths. If PJSIP is not found, configuration warns and falls back to stub mode.

---

## 10. Recommended Next Tasks

| Task | Recommended work | Priority |
|---|---|---|
| **22** | **Real PJSIP Integration Validation** — install PJSIP, enable `-DENABLE_PJSIP=ON`, smoke-test registration against a real SIP registrar, fix any compile or runtime issues in `SipAccount.cpp`/`SipCall.cpp`, expose the `pj::Account&` accessor in `SipAccount`, verify credential challenge flow, confirm audio path connects. **This should be the next task.** | Critical |
| 23 | Startup auto-register — call `registerActiveProfile()` automatically in `SipManager::initialize()` when the active profile has a stored credential. Add a UI option to disable this. | High |
| 24 | Network-change recovery — listen to `QNetworkInformation` (or equivalent) for connectivity events; re-trigger registration on recovery after a gap. | High |
| 25 | Incoming call notification — display an incoming-call popup/tray notification; allow accept/reject without the main window being focused. | Medium |
| 26 | Call history — persist non-secret call metadata (remote URI, duration, direction, timestamp) in a local SQLite or INI store; display in the History panel. | Medium |
| 27 | Debug bundle export — package redacted log entries, environment/build/Qt/PJSIP version info, and optional sanitized SIP traces into a zip archive; wire the "Bundle" button. | Medium |
| 28 | Audio level meters (live) — connect `SipCall` `inputLevelChanged` / `outputLevelChanged` signals to `MediaPanel` progress bars; implement a fallback polling timer in stub mode. | Low |
| 29 | RFC 4103 RTT skeleton — add T.140 session abstraction, SDP `text/t140` capability scaffolding, and a minimal send/receive test without claiming ETSI interoperability. | Low |
| 30 | Linux/macOS credential backends — implement `SecretServiceCredentialBackend` (libsecret) for Linux and `KeychainCredentialBackend` (Security.framework) for macOS. | Low |

---

## 11. Continuation Prompt for a New Chat

Paste this prompt verbatim into a new Claude Code session to resume work from this handoff.

```
Continue development of "SIP Client Audio Video RTT".
Repository: https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Goal: Build a generic cross-platform Windows/Linux SIP multimedia desktop client with SIP registration, audio/video calls, diagnostics, media device management, and SIP profiles. Future optional features include RFC 4103 RTT and ETSI TS 103 698/103 479/103 480.

Stack: C++17 + Qt 6 + CMake + optional PJSIP/pjsua2. Default build uses ENABLE_PJSIP=OFF (stub mode); no PJSIP dependency is required for compilation or tests.

Architecture constraints:
- Passwords NEVER in QSettings, SipProfile, logs, or signals — OS keychain only (CredentialStore)
- Platform guards before Qt headers: use #ifdef _WIN32, NOT #ifdef Q_OS_WIN
- HAVE_PJSIP guards all pjsua2 code; only SipManager.cpp, SipAccount.cpp, SipCall.cpp may include pjsua2 headers
- All PJSIP callbacks dispatch to the Qt main thread via QMetaObject::invokeMethod(Qt::QueuedConnection)
- CredentialStore key format: SIPClient/sip/<profileId>/<username> in Windows Credential Manager
- Test isolation: unique QCoreApplication org/app name per test file to avoid polluting real QSettings
- RAW log level default always false

Completed tasks (20):
 1  Project skeleton — Qt 6/CMake/C++17 desktop shell
 2  GUI layout specification (docs only, separate lineage)
 3  Diagnostics logger (separate lineage, not merged into active branch)
 4  Diagnostics GUI console (separate lineage, not merged)
 5  Persistent settings (separate lineage, not merged)
 6  SIP profile model — SipProfile, SipProfileManager, SidebarPanel
 7  Secure credential storage — CredentialStore, WindowsCredentialBackend, MemoryCredentialBackend
 8  SIP profile editor dialog — SipProfileEditorDialog, Add/Edit/Delete
 9  Media device enumeration — MediaDeviceManager, MediaDeviceSelectionModel, MediaPanel
10  PJSIP build integration skeleton — FindPJSIP.cmake, ENABLE_PJSIP, SipManager endpoint lifecycle
11  Project handoff 001 — formal project snapshot
12  SIP Register/Unregister — active profile + CredentialStore credential, SipAccount pjsua2 wrapper, Qt-thread callbacks, GUI status
13  Registration state machine — RegistrationStateMachine (5 states), tryTransition() guards, 30s watchdog, button disable
14  Registration retry/backoff — RegistrationRetryPolicy, isRetryable(code), delayForAttempt(n) exponential backoff, retryScheduled signal
15  Registration expiry refresh — RegistrationRefreshConfig, 80%-ratio/30s-margin timer, refresh→retry on failure, registrationStatusText
16  Profile switch sequencing — switchActiveProfile(), pending-switch guard, UNREGISTER old before registering new, profileSwitchStarted/Completed/Failed signals
17  SIP call state machine — CallStateMachine (9 states), SipCall API, SipManager call control, CallPanel wiring
18  Audio media integration — AudioMediaManager, mute/unmute, level forwarding, PJSIP bridge scaffolding
19  Video media integration — VideoMediaManager, video mute, camera selection, VideoPanel overlays, PJSIP bridge scaffolding
20  SIP diagnostics & ladder — SipMessageTrace, SipTraceLogger (credential redaction), SipLadderWidget, DiagnosticsPanel SIP tab, exportToText/exportToJson

Current branch: feature/project-handoff-002
Last feature commit: 0f94065 (Task 20, feature/sip-diagnostics-ladder)
Branch note: no main branch exists; origin/HEAD → feature/project-skeleton; each task branch is a linear child of the previous one.

Test status: 157 functional test methods across 14 suites, all passing in stub mode on Windows (Qt 6.11.1 / MSVC 2022).

Build environment (Windows):
  VS 2022 dev shell: C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1 -Arch amd64 -SkipAutomaticLocation
  Qt: F:\Programs\Qt\6.11.1\msvc2022_64
  Generator: NMake Makefiles
  Test output: <exe>.exe -o results.xml,xml  (PowerShell does not capture Qt test stdout)

IMPORTANT WARNING: Every PJSIP code path (registration challenge flow, call INVITE/SDP, audio/video media bridge, real SIP trace capture) has been implemented and compiled but never executed against a live PJSIP installation or SIP server. Real PJSIP validation is outstanding.

Read docs/project-handoff/handoff-002.md and docs/project-status.md before changing code.

Next recommended task: Task 22 — Real PJSIP Integration Validation.
Install PJSIP, build with -DENABLE_PJSIP=ON -DPJSIP_DIR=<path>, smoke-test registration against a real SIP registrar, fix any compile/runtime issues in SipAccount.cpp and SipCall.cpp, expose the pj::Account& accessor in SipAccount (current TODO), verify the 401 credential challenge flow, confirm audio path connects. Do not implement new features in this task — validation and bug fixes only.
```
