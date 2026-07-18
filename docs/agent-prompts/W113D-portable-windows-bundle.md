# Agent Prompt — Task W113D: Generate Portable Windows Test Bundle

Condensed from the full task spec provided by the project owner in chat —
this file summarizes the task's requirements rather than duplicating every
line.

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `release/w113d-portable-windows-bundle`.
- Starting branch: `fix/w113c-camera-led-stays-on` (complete, 80/80 CTest,
  pushed without merge).
- No merge into `main`/`release`. `pjproject` sources not modified. No
  product-feature changes except what packaging/deployment strictly
  requires.

## Objective

Produce a self-contained, portable Windows x64 Release bundle of SIPClient
that runs on a machine with no Qt, Visual Studio, or PJSIP dev environment
installed — copy the zip, unzip, run `SIPClient.exe`.

## Versioning

PATCH bump 1.6.3 → 1.6.4 (new distribution artifact, no behavior change).
Application/backend/frontend version, Windows FILEVERSION/PRODUCTVERSION,
CMake project version, startup log, About/System Info, and diagnostics
export must all report 1.6.4 coherently — SIPClient is a single binary with
one `PROJECT_VERSION`, so this is one CMakeLists.txt edit plus a new
Windows version-resource (`cmake/AppVersion.rc.in`) rather than N separate
edits.

Artifact name: `SIP-Client-Audio-Video-RTT-1.6.4-windows-x64-portable.zip`,
folder `SIP-Client-Audio-Video-RTT-1.6.4-windows-x64/`.

## Required phases (full text abbreviated; see chat history for the
## complete 14-phase spec)

1. Baseline: confirm toolchain/Qt/PJSIP/arch/windeployqt location; Debug +
   Release build + CTest + startup smoke test.
2. Clean Release x64 build tree (no hardcoded local paths — resolved from
   CMake cache/environment/script parameters).
3. Clean staging directory — exe, README-PORTABLE.txt, version manifest;
   excludes PDBs, source, build cache, tests, credentials, personal config,
   logs, captures, dumps, local paths.
4. Qt deployment via `windeployqt`; verify plugins actually used
   (platforms, multimedia, styles, imageformats, iconengines, tls,
   networkinformation) are present.
5. Non-Qt dependency audit (`dumpbin /dependents`) for PJSIP/OpenSSL/
   zlib/codecs/FFmpeg/custom DLLs/MSVC runtime; no unverified
   redistributables; no System32 copies.
6. First-run config: no personal/default SIP account, no admin privileges,
   no auto packet-capture, no test-control API, no experimental MSRP
   relay/LMPE by default; `README-PORTABLE.txt` documents requirements,
   startup, config location, `--config-dir`/`SIPCLIENT_CONFIG_DIR`,
   camera/mic permissions, firewall prompt, VC++ runtime, diagnostics
   collection, version, known limitations.
7. `version-info.json` (product, applicationVersion, backendVersion,
   frontendUiVersion, schemaVersion, buildType, architecture, gitCommit,
   qtVersion, pjsipVersion) — generated from the build, not hardcoded, no
   build path/username/hostname/credentials.
8. Reproducible `scripts/package-windows.ps1` (parameterized: BuildDir,
   Configuration, QtBinDir, OutputDir, Version, IncludeSymbols,
   IncludeVcRedist, Clean, Archive) that validates, stages, deploys, audits,
   generates the manifest, scans for secrets, verifies DLLs, smoke-tests,
   zips, checksums, and fails with a non-zero exit on any critical problem
   — no continue-on-error masking.
9. Smoke test from **staging**, not the build tree, with `PATH` reduced so
   the app can't accidentally resolve build-machine Qt/VS DLLs.
10. Clean-machine test (VM/Sandbox/second PC) — full REGISTER/audio/video/
    RTT/messaging/Presence/diagnostics/restart-persistence pass, reported
    strictly as PASS/FAIL/BLOCKED/NOT RUN/UNSUPPORTED. Not claimed as
    validated if no clean machine is actually available.
11. Security/hygiene scan of staging + zip for secrets, local paths,
    dangerous archive entries, architecture mixing, Debug DLLs in Release.
12. Final artifacts: portable zip, `.sha256`, manifest JSON, optional
    symbols zip, packaging result report.
13. Documentation: `windows-portable-bundle.md`,
    `windows-deployment-dependencies.md`, `windows-clean-machine-test.md`,
    this file, the result file; update README, project-status,
    versioning-and-rollout, release-notes.
14. Acceptance: bundle must not depend on the build machine's Qt/VS PATH,
    must include all required plugins/DLLs, must exclude Debug DLLs/wrong
    architecture/credentials/personal config, version must actually be
    1.6.4 everywhere, zip must have a checksum and manifest, packaging must
    be reproducible via the script, smoke test must run the staged exe (not
    the build-tree one), and an unexecuted clean-machine test must be
    reported as NOT RUN/BLOCKED — never PASS.

## Git

`git push -u origin release/w113d-portable-windows-bundle` at the end; no
merge into `main` or `release`.

See [W113D-portable-windows-bundle-result.md](../agent-results/W113D-portable-windows-bundle-result.md)
for the full 50-item final report.
