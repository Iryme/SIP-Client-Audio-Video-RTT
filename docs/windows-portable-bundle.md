# Windows Portable Bundle

How to build a self-contained, portable Windows x64 Release bundle of
SIPClient for testing on a machine with no Qt, Visual Studio, or PJSIP dev
environment installed. Produced by
[scripts/package-windows.ps1](../scripts/package-windows.ps1). See
[windows-deployment-dependencies.md](windows-deployment-dependencies.md) for
what actually goes into the bundle and why, and
[windows-clean-machine-test.md](windows-clean-machine-test.md) for the
test procedure to run once you have it on a second machine.

## Prerequisites (build machine only — not needed on the target machine)

- A configured Release x64 CMake build tree with `ENABLE_PJSIP=ON`
  (see [build-windows.md](build-windows.md)).
- A Visual Studio x64 developer shell (`vcvars64.bat`) — the script needs
  `dumpbin.exe` on `PATH` for the dependency audit.
- The Qt install used to configure that build tree (the script resolves
  `windeployqt.exe`'s location from the build tree's own `Qt6_DIR` cache
  entry — nothing Qt-related is hardcoded in the script or the repo).

## Building the bundle

```powershell
# From a VS x64 developer shell, at the repo root:
cmake -S . -B build-windows-x64-release -G "NMake Makefiles" `
    -DCMAKE_BUILD_TYPE=Release -DENABLE_PJSIP=ON `
    -DPJSIP_DIR=.deps/pjsip-msvc-install-release -DBUILD_TESTS=ON
cmake --build build-windows-x64-release --parallel
ctest --test-dir build-windows-x64-release --output-on-failure

powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1 `
    -BuildDir build-windows-x64-release -IncludeSymbols
```

(`-G "NMake Makefiles"` matches this project's established single-config
toolchain — a multi-config generator like `"Visual Studio 17 2022"` works too,
just point `-BuildDir` at the `Release` output subfolder it produces.)

### What the script does

1. Validates `-BuildDir` is a configured build tree with a built
   `SIPClient.exe`; resolves Qt/`windeployqt` from its CMake cache.
2. Recreates a clean staging directory:
   `dist/staging/SIP-Client-Audio-Video-RTT-<version>-windows-x64/`.
3. Copies `SIPClient.exe` (+ manifest) and a generated `README-PORTABLE.txt`
   (from `scripts/README-PORTABLE.template.txt`) into staging — no `.pdb`,
   no source, no build cache, no test binaries, no personal config.
4. Runs `windeployqt --release --compiler-runtime --no-translations` against
   the staged exe, then verifies `platforms/` and `multimedia/` (and
   `qwindows.dll` specifically) exist — hard failure otherwise.
5. Audits every DLL in staging with `dumpbin /dependents`, failing the build
   if any binary references a non-system DLL that isn't already present in
   staging (see [windows-deployment-dependencies.md](windows-deployment-dependencies.md)
   for why PJSIP/OpenSSL/vpx need no entry here — they're statically linked).
6. Includes `vc_redist.x64.exe` alongside `--compiler-runtime`'s local CRT
   deployment (belt-and-suspenders; documented in the README).
7. Writes `version-info.json` (application/backend/frontend version, schema
   version, build type, architecture, git commit, Qt version, PJSIP version)
   — generated from the build tree, never hand-typed.
8. Scans every text/JSON/config file in staging for credentials, SIP
   `Authorization`/`Proxy-Authorization` headers, private keys, and the
   build machine's own username/hostname/repo path — hard failure on any
   hit.
9. Verifies every DLL is x64 and that no Debug-suffixed Qt/CRT DLL
   (`Qt6*d.dll`, `MSVCP*d.dll`, `VCRUNTIME*d.dll`) made it into a Release
   bundle.
10. Smoke-tests the **staged** `SIPClient.exe` (never the build-tree copy)
    with `PATH` reduced to `%SystemRoot%\System32` only, so it can't
    accidentally resolve a Qt/VS DLL still present on the build machine.
    Confirms the process starts and closes cleanly within a few seconds.
11. Zips staging to
    `dist/SIP-Client-Audio-Video-RTT-<version>-windows-x64-portable.zip`,
    writes its SHA-256 alongside it, and writes a per-file manifest JSON.
12. Optionally (`-IncludeSymbols`) zips `SIPClient.pdb` into a separate
    `*-symbols.zip` — a private artifact, not meant to ship with the public
    bundle.

Any critical failure exits non-zero — nothing is masked with
`-ErrorAction SilentlyContinue` or swallowed.

## Using the bundle on another machine

1. Copy `SIP-Client-Audio-Video-RTT-<version>-windows-x64-portable.zip` and
   its `.sha256` file to the target machine.
2. Verify the checksum, then unzip.
3. Run `SIPClient.exe` from inside the unzipped folder — see
   `README-PORTABLE.txt` inside the bundle for first-run behavior, config
   location, multi-profile use, camera/mic permissions, and known
   limitations.

For a rigorous test pass (REGISTER, audio/video call, RTT, camera LED,
messaging, diagnostics export, restart/persistence), follow
[windows-clean-machine-test.md](windows-clean-machine-test.md).
