# W113H Result — Windows 1.6.8 Parity Build and Live Windows ↔ macOS Interoperability Validation

## Outcome

W113H is complete for the Windows-only scope this environment can actually
support. Windows Debug and Release builds at 1.6.8 both pass 81/81 CTest with
the real PJSIP backend, the portable Windows bundle was rebuilt and packaged
clean with a full dependency/secret/architecture audit and smoke test, and the
existing LMPE-disabled / no-raw-XML regression tests were confirmed still
passing. **No macOS device, second Windows machine, or SIP test server was
available in this session**, so every live Windows↔macOS interoperability
scenario (Fazele 5–13) is reported BLOCKED, not PASS. No scenario that wasn't
actually executed is declared PASS anywhere in this report.

## Source control and version

| Item | Result |
|---|---|
| Required base | PASS — `feature/w113g-macos-arm64-bundle` at `e82a496`, matches the required commit exactly |
| Working tree clean before branching | PASS |
| New branch | PASS — `test/w113h-windows-macos-live-interoperability`, created from `e82a496` |
| Application version | PASS — `1.6.8` (already set by W113G; no bump needed or performed) |
| pjproject source changes | PASS — none; confirmed via `git diff --stat 8fa1fb0..e82a496` touching only macOS bundle/keychain/docs paths, no `.deps`/pjproject files |
| Merge | NOT RUN — prohibited by the task |
| Tag | NOT RUN — prohibited by the task |

## Windows environment inventory (Faza 2)

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.26200, x64 |
| Visual Studio | 18 (Community) |
| MSVC toolset | 14.51.36231 |
| Windows SDK | 10.0.26100.0 / 10.0.28000.0 present |
| CMake | 4.2.3-msvc3 |
| Ninja | not on PATH — build uses NMake Makefiles generator, as in every prior Windows task this session |
| Qt | 6.11.1, `msvc2022_64` kit (x64) |
| PJSIP | 2.17.0 (`.deps/pjsip-msvc-install[-release]`) |
| OpenSSL | **not linked** — PJSIP's own CMake configure recorded `OPENSSL_INCLUDE_DIR-NOTFOUND`/`OpenSSL_DIR-NOTFOUND` and `PJ_HAS_SSL_SOCK 0` in the installed `config.h`/`os_auto.h`. This build of PJSIP has no TLS transport compiled in at all — a pre-existing build-configuration fact, not something this task changed. |
| Build generator | NMake Makefiles (Debug: `build/`, Release: `build-release/`, packaging: `build-windows-x64-release/`) |
| Deployment environment | Local developer machine; no CI configured for this repository |

No local paths, hostnames, or credentials were written into the repository as
part of this inventory.

## Windows build and test (Faza 3)

| Configuration | Configure | Build | CTest |
|---|---|---|---|
| Debug (`build/`) | PASS — "PJSIP found — building with real SIP backend" | PASS | **81/81 PASS** |
| Release (`build-release/`) | PASS | PASS | **81/81 PASS** |

No test was skipped or disabled to obtain a green run. The 81-test suite
includes, among others: `test_sip_profile_manager` (LMPE forced disabled on
profile load), `test_lmpe_codec_unconfirmed` (LMPE inert at codec level),
`test_message_history` / `test_conversation_model` (protocol-event exclusion
from Client chat / no-raw-XML-in-history), `test_msrp_*`, `test_conversation_model`,
Python/parser/config/version tests already wired into this CTest suite.

Version metadata verified consistent across all surfaces sourced from the
single `PROJECT_VERSION` in `CMakeLists.txt`:

- `generated/AppVersion.h`: `APP_VERSION_STRING "1.6.8"`, `APP_GIT_COMMIT_HASH "e82a496"`, correct `APP_BUILD_TYPE_STRING` per configuration, in both `build/` and `build-release/`.
- `generated/AppVersion.rc`: `FILEVERSION 1,6,8,0` / `PRODUCTVERSION 1,6,8,0`.
- `src/main.cpp`, `src/gui/MainWindow.cpp` (About/System Information), `src/app/Application.cpp`, `src/core/DiagnosticsCollector.cpp` (diagnostics export) all reference the same `APP_VERSION_STRING` — one source of truth, no drift possible between startup log, About dialog, and diagnostics export.
- Interactive click-through of the About dialog/System Information panel itself is NOT RUN (no GUI automation available this session, consistent with every prior task) — the source-level consistency above is what was actually verified.

No macOS-only source (`MacKeychainBackend.mm`/`.cpp`, `Info.plist.in`-only
paths) was compiled into the Windows targets — `ENABLE_PJSIP`/platform guards
in `CMakeLists.txt` route those files out of the Windows build; confirmed by
inspecting the generated NMake build file target lists, which contain no
macOS-only translation units.

## Windows bundle (Faza 4)

`build-windows-x64-release` was reconfigured/rebuilt at `1.6.8` (previously
stale at `1.6.4`, predating W113E/F/G) and packaged via
`scripts\package-windows.ps1`:

| Check | Result |
|---|---|
| Qt deployment (`windeployqt --release --compiler-runtime`) | PASS — Core/Gui/Multimedia/Network/Svg/Widgets + platforms/multimedia/styles/imageformats/iconengines/tls/networkinformation/generic plugins deployed |
| VC++ runtime | PASS — copied directly from `Microsoft.VC145.CRT` redist (not relying on `--compiler-runtime` alone, per the script's documented rationale) |
| Non-Qt dependency audit (`dumpbin /dependents`) | PASS — no unresolved non-system DLLs |
| PJSIP/OpenSSL/zlib/vpx | Statically linked into `SIPClient.exe` — no runtime DLL to ship, as expected (and consistent with the "OpenSSL not linked" finding above: PJSIP's own build has no OpenSSL/TLS backend compiled in, so there is nothing to bundle for it) |
| Secret/local-path scan | PASS — no username, computer name, repo path, or credential pattern found in staged text files |
| Architecture/Debug-DLL check | PASS — all staged binaries x64, no Debug-suffixed Qt/CRT DLLs |
| Smoke test (staging, minimal PATH) | PASS — process started and closed cleanly with no Qt/VS DLLs resolvable from the build machine |
| Version displayed | PASS — `version-info.json`: `applicationVersion`/`backendVersion`/`frontendUiVersion` all `1.6.8`, `gitCommit` `e82a496`, `qtVersion` `6.11.1`, `pjsipVersion` `2.17.0` |
| Config persistence / restart / clean shutdown / diagnostics export | NOT RUN — these require an interactive GUI session; only the non-interactive smoke test (process start/close) was run, consistent with every prior Windows task this session |
| No local build path in manifest/logs | PASS — covered by the same secret/local-path scan |
| Artifacts | PASS — `dist/SIP-Client-Audio-Video-RTT-1.6.8-windows-x64-portable.zip` (SHA-256 `c7758d3a9745e427e9fc479c7ea4c70d5b4691ef53f392a6b1b2d8da5ca7903d`) + `.sha256` sidecar + `-manifest.json`. Symbols archive NOT RUN (not requested by the default script invocation this run; `SIPClient.pdb` exists in the build tree if a follow-up run wants `-IncludeSymbols`) |

## LMPE and raw-XML regression (explicit re-confirmation)

| Check | Result |
|---|---|
| LMPE forced unavailable at every UI surface (profile editor, RttPanel, CallWorkspacePanel status card) | PASS — unchanged since W113F, `test_lmpe_codec_unconfirmed` + `test_sip_profile_manager`'s `enableLmpeForcedFalseOnLoadRegardlessOfSavedValue` pass on this build |
| Client Messaging never shows raw XML/CPIM/IMDN/is-composing payload | PASS — unchanged since W113F, `test_message_history`/`test_conversation_model`'s `userVisibleHistoryExcludes*` tests pass on this build |

No code was modified to reach these results — this task only re-verified
W113F's fixes still hold at 1.6.8 on Windows.

## Feature matrix (Fazele 6–13)

| Feature | Windows | macOS | Win→Mac | Mac→Win | Status | Evidence | Notes |
|---|---|---|---|---|---|---|---|
| REGISTER | NOT RUN (no live server) | NOT RUN (no macOS device) | BLOCKED | BLOCKED | BLOCKED | — | No SIP server/accounts provisioned in this environment |
| UDP transport | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires a live REGISTER first |
| TCP transport | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires a live REGISTER first |
| TLS transport | UNSUPPORTED | N/A | UNSUPPORTED | UNSUPPORTED | UNSUPPORTED | `PJ_HAS_SSL_SOCK 0` in this Windows PJSIP build | Not a live-test gap — this PJSIP build has no TLS backend compiled in at all (no OpenSSL found at PJSIP configure time). A W113G macOS PJSIP build does link OpenSSL per that task's report, so this is a Windows-build-specific limitation, not symmetric |
| Audio (bidirectional) | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | No live call possible without a registered peer |
| Video (bidirectional) | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Same; also see W113G's own report that its PJSIP build had no video codec enumerated |
| Camera on/off | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires an active video call |
| Microphone | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires an active audio call |
| RTT (RFC 4103/T.140) | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires an active call with a live peer |
| RequestBlinker (visual alert parity) | PASS (unit, unchanged) | N/A | N/A | N/A | PASS (structural only) | `test_request_blinker` passes in this build's CTest run | Confirms the W113E widget still builds/passes; not a live incoming-request click-through |
| SIP MESSAGE | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | No live peer |
| CPIM | PASS (unit, unchanged) | N/A | BLOCKED | BLOCKED | PASS (structural) / BLOCKED (live) | `test_message_history`/`test_conversation_model` pass | Routing logic verified by test, not by a live CPIM-wrapped message between real peers |
| IMDN | PASS (unit, unchanged) | N/A | BLOCKED | BLOCKED | PASS (structural) / BLOCKED (live) | Same suite | Same caveat |
| is-composing | PASS (unit, unchanged) | N/A | BLOCKED | BLOCKED | PASS (structural) / BLOCKED (live) | Same suite | Same caveat |
| Presence/SUBSCRIBE-NOTIFY | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | No Presence server available |
| XCAP | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | No XCAP server available |
| MSRP direct | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | No live peer |
| MSRP relay | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | No relay server available; per the task's own rule, relay is never declared PASS without an actual relay used |
| File transfer (MSRP) | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires an established MSRP session |
| Hold/resume | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires an active call |
| re-INVITE | NOT RUN | NOT RUN | BLOCKED | BLOCKED | BLOCKED | — | Requires an active call |
| Diagnostics export | PASS (Windows, non-interactive) | N/A (not this task) | N/A | N/A | PASS (Windows structural) | `DiagnosticsCollector.cpp` sources verified to report `1.6.8`/`e82a496`; interactive export click-through NOT RUN | No SIP Ladder/live-call events exist to export without a real call |
| Restart | NOT RUN | N/A | N/A | N/A | NOT RUN | — | Requires interactive GUI session |
| Config persistence | NOT RUN | N/A | N/A | N/A | NOT RUN | — | Requires interactive GUI session |
| LMPE (unavailable everywhere) | PASS | N/A (not this task) | PASS | PASS | PASS | `test_lmpe_codec_unconfirmed`, `test_sip_profile_manager` | Confirmed structurally unavailable; no live SDP/payload check possible without a call, but the codec is inert at the source level regardless of call state |
| Raw XML in Client chat | PASS (none observed) | N/A (not this task) | N/A | N/A | PASS | `test_message_history`/`test_conversation_model` filtering tests | Same caveat as CPIM/IMDN/is-composing above — structural, not live |

## Defects (Faza 14)

None found or reproduced this task. No code was changed. This task's entire
executable scope was re-verification of existing behavior at the current
version/commit, not new feature or fix work — the "implement minimal fix only
if a real defect is reproduced" rule was honored by making zero source
changes.

## Remaining blockers to real interoperability validation

Before Fazele 5–13 can produce real PASS/FAIL results: provision a macOS
ARM64 device (or reuse the one from W113G) alongside this Windows machine at
the same time, stand up a SIP test server with two disposable accounts (UDP
at minimum; TCP/TLS if the server supports them — noting this Windows build
currently has no TLS transport compiled into PJSIP at all, so a TLS scenario
would additionally need a PJSIP rebuild with OpenSSL located), and optionally
a Presence/XCAP server and MSRP relay if those scenarios are in scope. None of
this infrastructure exists in either this session or the W113G session that
preceded it.
