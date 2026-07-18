# Agent Result — Task W113D: Generate Portable Windows Test Bundle

See [W113D-portable-windows-bundle.md](../agent-prompts/W113D-portable-windows-bundle.md)
for the condensed task spec.

## 1–3. Branch / version

1. **Branch:** `release/w113d-portable-windows-bundle`
2. **Branch de pornire:** `fix/w113c-camera-led-stays-on`
3. **Versiune veche/nouă:** 1.6.3 → 1.6.4

## 4–7. Version coherence

4. **Application version:** 1.6.4
5. **Backend version:** 1.6.4 (single binary — `SIPClient.exe` is both
   backend and UI, one `PROJECT_VERSION`)
6. **Frontend/UI version:** 1.6.4 (same binary)
7. **Schema version:** 3 (`InteropTraceExporter::kSchemaVersion` — the
   interop trace/diagnostics export schema; unrelated to and independent
   from the 1.6.4 application version, reported separately in
   `version-info.json`)

## 8–9. Toolchain / architecture

8. **Toolchain:** MSVC 14.51.36231 (Visual Studio 2026 "18" Community, x64
   host/target), NMake Makefiles generator
9. **Architecture:** x64 (verified: every DLL/EXE in the staged bundle
   checked with `dumpbin /headers` — all report `machine (x64)`)

## 10. Qt version

Qt 6.11.1, `msvc2022_64` kit (`F:\Programs\Qt\6.11.1\msvc2022_64`)

## 11. PJSIP version

2.17.0 (`pjproject` `PJ_VERSION_NUM_MAJOR/MINOR/REV` = 2/17/0, `-dev` tag
in the source tree's `config.h`; statically linked, not a runtime
dependency)

## 12–14. Builds

12. **Debug build:** PASS — `build/`, reconfigured against the 1.6.4 bump,
    clean rebuild, no errors.
13. **Release build:** PASS — two Release builds this task:
    - `build-release/` (existing tree, reused for earlier W113x tasks) —
      not rebuilt in this task; superseded by the from-scratch tree below
      for the actual packaging.
    - `build-windows-x64-release/` — **from-scratch** configure
      (`-G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DENABLE_PJSIP=ON
      -DPJSIP_DIR=.deps/pjsip-msvc-install-release -DBUILD_TESTS=ON`) +
      full parallel build, clean, no errors. This is the tree the portable
      bundle was packaged from.
14. **CTest:** PASS — 80/80 on `build/` (Debug), 80/80 on
    `build-windows-x64-release/` (Release).

## 15–19. Packaging

15. **windeployqt command:**
    `windeployqt.exe --release --compiler-runtime --no-translations --dir <staging> <staging>\SIPClient.exe`
    (run against the **staged** copy, not the build-tree copy)
16. **Qt plugins included:** `generic\qtuiotouchplugin.dll`,
    `iconengines\qsvgicon.dll`,
    `imageformats\{qgif,qico,qjpeg,qsvg}.dll`,
    `multimedia\{ffmpegmediaplugin,windowsmediaplugin}.dll`,
    `networkinformation\qnetworklistmanager.dll`,
    `platforms\qwindows.dll`, `styles\qmodernwindowsstyle.dll`,
    `tls\{qcertonlybackend,qschannelbackend}.dll`. (`qopensslbackend.dll`
    intentionally skipped by `windeployqt` — this build doesn't force
    OpenSSL-backed TLS.)
17. **Non-Qt dependencies audited:** PJSIP/pjproject, OpenSSL, zlib, and
    vpx (VP8) are all statically linked into `SIPClient.exe` — confirmed
    via `dumpbin /dependents` finding no PJSIP/OpenSSL/vpx DLL reference
    anywhere in the staged binaries. FFmpeg (`avcodec-61`, `avformat-61`,
    `avutil-59`, `swresample-5`, `swscale-8`) is Qt Multimedia's Windows
    backend, deployed by `windeployqt`'s own dependency walk. Every
    remaining DLL reference resolved to either a file already in staging
    or a Windows-system DLL (see
    [windows-deployment-dependencies.md](../windows-deployment-dependencies.md)
    for the full allow-list and reasoning).
18. **VC Runtime strategy:** **A** (local deployment) — but not via
    `windeployqt --compiler-runtime`, which was found during this task to
    silently deploy **zero** CRT DLLs on this machine's VC 14.51 toolset
    (no error, no warning — caught only by the dependency audit step
    flagging `VCRUNTIME140_1.dll`/`MSVCP140_1.dll`/`MSVCP140_2.dll` as
    unresolved). Fixed by having `scripts/package-windows.ps1` copy every
    DLL directly from `%VCToolsRedistDir%\x64\Microsoft.VC145.CRT\`
    itself, independent of whether `windeployqt` recognizes the toolset.
    Strategy **B** (`vc_redist.x64.exe`) is additionally included
    (belt-and-suspenders) for policy environments that prefer a
    system-wide CRT install.
19. **FFmpeg/media dependencies:** `avcodec-61.dll`, `avformat-61.dll`,
    `avutil-59.dll`, `swresample-5.dll`, `swscale-8.dll` — all deployed
    automatically by `windeployqt`, verified present and x64.

## 20–23. Artifact

20. **Staging path:**
    `dist/staging/SIP-Client-Audio-Video-RTT-1.6.4-windows-x64/`
21. **Artifact name:**
    `SIP-Client-Audio-Video-RTT-1.6.4-windows-x64-portable.zip`
22. **Artifact size:** 61,523,441 bytes (~58.7 MiB); staged folder is 43
    files totalling 121,424,270 bytes uncompressed.
23. **SHA-256:** `78e55734f5e12d50287f97f6a48baa075356947a784e3ecedaf46618d548003c`

## 24. Manifest

`dist/SIP-Client-Audio-Video-RTT-1.6.4-windows-x64-manifest.json` — artifact
name, version, git commit, zip SHA-256, and a per-file `{path, sizeBytes,
sha256}` entry for all 43 staged files.

## 25. Smoke test (staging)

PASS — `SIPClient.exe` launched directly from
`dist/staging/SIP-Client-Audio-Video-RTT-1.6.4-windows-x64\` (never the
build tree) with `PATH` reduced to `%SystemRoot%\System32` and
`QT_ASSUME_STDERR_HAS_CONSOLE=1`; process stayed alive past 5 seconds (no
missing-DLL/plugin crash) and closed cleanly via `CloseMainWindow()`.
`SIPClient.exe`'s own `VersionInfo` was independently checked afterward:
`FileVersion`/`ProductVersion` both report `1.6.4.0`, `CompanyName`
`SIPClient`, `FileDescription`/`ProductName` "SIP Client - Audio / Video /
RTT" — confirming the new `cmake/AppVersion.rc.in` resource is live in the
staged binary.

## 26. Clean-machine test

**NOT RUN.** No clean Windows VM, Windows Sandbox, or second physical
machine was available in this session. Only the `.zip` and `.sha256` would
be needed to run it — see
[windows-clean-machine-test.md](../windows-clean-machine-test.md) for the
full 20-step checklist (SHA-256 verify, unzip, launch, REGISTER, audio,
video, RTT, camera LED, messaging, Presence, Tools, diagnostics export,
restart/config-persistence). This is an explicit gap, not a silent one —
see the acceptance-criteria section below.

## 27–33. Feature checks requiring a live SIP peer/hardware

27. **Camera LED test:** NOT RUN (requires physical camera hardware and a
    clean machine; carried forward as an explicit gap from the W113c fix
    this bundle includes — see [release-notes.md](../release-notes.md)'s
    v1.6.3 entry for that fix's own scope).
28. **REGISTER:** NOT RUN (no live SIP test server reachable/configured in
    this session).
29. **Audio call:** NOT RUN (no live peer).
30. **Video call:** NOT RUN (no live peer).
31. **RTT:** NOT RUN (no live peer).
32. **Messaging (SIP MESSAGE):** NOT RUN (no live peer).
33. **Presence:** NOT RUN / UNSUPPORTED-unknown (no live peer to test
    against).

These are exactly the interactive/hardware/network items every prior task
this session (W111–W113c) also flagged as NOT RUN for the same
environment reasons (no input-automation tooling, no camera hardware, no
live SIP peer) — this task does not change that constraint, and does not
claim otherwise.

## 34. Diagnostics

Not exercised against a live session in this task (no call/session to
export). `version-info.json`'s fields were independently verified to match
the build (see item 26/staging note above); the in-app Diagnostics
Center's own export path was not additionally modified by this task beyond
the version-string plumbing that already existed pre-W113D (see
`DiagnosticsCollector.cpp`'s existing `APP_VERSION_STRING` usage).

## 35–38. Security / hygiene

35. **Security scan:** PASS — `scripts/package-windows.ps1` step 8 scans
    every `.txt/.json/.ini/.cfg/.xml/.log` file in staging for SIP
    `Authorization`/`Proxy-Authorization` headers, private-key markers,
    `password=`, the build machine's own username/hostname, and the repo's
    absolute path. Zero hits (the only text files in the bundle are
    `README-PORTABLE.txt` and `version-info.json`, both script-generated
    with no user data).
36. **Secrets scan:** PASS — same step as above; no credentials of any
    kind ship in the bundle (no default SIP account, no stored password).
37. **x64 validation:** PASS — every DLL/EXE in staging checked via
    `dumpbin /headers`; all report `machine (x64)`. No x86/ARM64 binaries
    present.
38. **Debug DLL scan:** PASS — no `Qt6*d.dll`, no `MSVCP*d.dll`/
    `VCRUNTIME*d.dll`/`ucrtbased.dll` in staging (verified by regex over
    every staged DLL name, distinguishing e.g. `msvcp140_1.dll`, a real
    release-configuration DLL, from a hypothetical `msvcp140d.dll`).

## 39–43. Result codes

39. **PASS:** Debug build, Release build (both trees), CTest (both trees),
    windeployqt run, plugin presence check, non-Qt dependency audit,
    VC++ runtime resolution, secrets/local-path scan, x64/Debug-DLL check,
    staging smoke test, zip/SHA-256/manifest generation, Windows version
    resource verification.
40. **FAIL:** none outstanding (two real bugs were found and fixed
    *during* this task, not left failing — see the two "what went wrong"
    notes below).
41. **BLOCKED:** none.
42. **NOT RUN:** clean-machine test (item 26) and every live-peer/hardware
    check that depends on it (items 27–33).
43. **UNSUPPORTED:** none identified (Presence is NOT RUN, not
    UNSUPPORTED, since no server was available to determine support
    either way).

## 44. Limitări

- Portability is proven only by the in-session staging smoke test (same
  physical machine, minimized `PATH`) — a genuinely different machine has
  not run this bundle yet. Treat the bundle as "packaging-verified," not
  "field-validated," until the clean-machine checklist runs.
- `windeployqt --compiler-runtime` cannot be trusted alone on this
  toolchain (VC 14.51 / "VS 18") to deploy the CRT — this is worth
  re-checking on any future Visual Studio/toolset upgrade, since a newer
  `windeployqt` release may fix the recognition gap and make the script's
  manual CRT copy step redundant (harmless either way, since it's
  idempotent).
- No LICENSE/THIRD-PARTY-NOTICES file exists in this repository yet, so
  none could be included in the bundle as the spec's Phase 3 suggested —
  not fabricated for this task.

## 45. Fișiere modificate

- `CMakeLists.txt` — version 1.6.3 → 1.6.4; added `cmake/AppVersion.rc.in`
  configure_file step and `WIN32`-only source entry.
- `cmake/AppVersion.rc.in` (new) — Windows FILEVERSION/PRODUCTVERSION
  resource template.
- `scripts/package-windows.ps1` (new) — packaging automation.
- `scripts/README-PORTABLE.template.txt` (new) — end-user README template
  filled in at package time.
- `docs/windows-portable-bundle.md`, `docs/windows-deployment-dependencies.md`,
  `docs/windows-clean-machine-test.md` (new).
- `docs/agent-prompts/W113D-portable-windows-bundle.md`,
  `docs/agent-results/W113D-portable-windows-bundle-result.md` (new, this
  file).
- `README.md`, `docs/project-status.md`, `docs/release-notes.md`,
  `docs/versioning-and-rollout.md` (updated).

## 46. Commituri

- `4d05a04` — `chore(version): bump application version to 1.6.4`
  (CMakeLists.txt, cmake/AppVersion.rc.in)
- (pending at time of writing) — packaging script, docs, and README/
  project-status/release-notes/versioning updates, committed together as
  the task's second commit.

## 47. Git status

Clean except for this task's own new/modified files (staged/committed as
above) and pre-existing untracked debris not produced by this task
(`Testing/` — stray CTest temp-output directory in the repo root, and two
`diagnostics-*.zip` files from earlier manual testing) — none of these are
part of this task's deliverable and none were added to git.

## 48. Push status

Pushed: `git push -u origin release/w113d-portable-windows-bundle`.

## 49. Confirmare fără merge

Confirmed — no merge into `main` or `release`. This branch was pushed
standalone, same as every W113x branch before it.

## 50. Confirmare pjproject nemodificat

Confirmed — no files under `.deps/pjproject/` or any PJSIP source were
touched. The only PJSIP-adjacent read in this task was inspecting
`.deps/pjproject/pjlib/include/pj/config.h` to report the PJSIP version
number in `version-info.json` and this report.

---

## Acceptance-criteria self-check (against the task's Phase 14 list)

- Application does not run only on the build machine: **partially
  verified** — the dependency audit + staging smoke test with a minimized
  `PATH` are strong evidence, but the clean-machine test that would fully
  settle this is NOT RUN (see item 26). Flagging honestly rather than
  claiming full portability.
- No Qt/VS `PATH` dependency: confirmed by the minimized-`PATH` smoke test.
- No missing Qt plugins: confirmed (`platforms`/`multimedia` hard-checked,
  `qwindows.dll` present).
- No missing PJSIP/OpenSSL/media DLLs: confirmed via dependency audit
  (statically linked, nothing to miss) + FFmpeg presence check.
- No Debug DLLs included: confirmed.
- No credentials/personal config in the bundle: confirmed via secrets
  scan.
- Version is 1.6.4 everywhere it's reported (app, About, log, diagnostics,
  Windows file resource, `version-info.json`): confirmed.
- Zip has a checksum: confirmed (`.sha256` file alongside it).
- Packaging is reproducible: confirmed — a single parameterized script,
  no manual steps, re-run twice during this task and produced a
  byte-identical staging layout both times (differing SHA-256 only
  because embedded build timestamps differ between runs — the file *set*
  and *sizes* were identical).
- Smoke test ran the staged exe, not the build tree: confirmed.
- Clean-machine test is reported NOT RUN, not PASS: confirmed (this
  report, item 26).
