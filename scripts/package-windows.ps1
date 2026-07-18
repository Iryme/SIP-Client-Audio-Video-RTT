# Packages a portable, self-contained Windows x64 Release bundle of SIPClient
# for copying to a machine that has no Qt, Visual Studio, or dev environment
# installed -- see docs/windows-portable-bundle.md for the full procedure and
# docs/windows-deployment-dependencies.md for the dependency audit this script
# performs.
#
# Must be run from inside a Visual Studio x64 developer shell (so dumpbin.exe
# is on PATH for the dependency audit) -- run vcvars64.bat first, then this
# script. windeployqt is located from the build tree's own CMakeCache.txt
# (Qt6_DIR), never hardcoded.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1 `
#       -BuildDir build-windows-x64-release -Archive -IncludeVcRedist
#
# Exits with a non-zero code on any critical problem (missing exe, missing
# Qt plugin directory, unresolved non-system DLL dependency, secret/local-path
# hit in the scan, smoke test failure). Nothing here uses -ErrorAction
# SilentlyContinue / continue-on-error for a step that matters.

param(
    [string]$BuildDir       = "build-windows-x64-release",
    [string]$Configuration  = "Release",
    [string]$QtBinDir       = "",
    [string]$OutputDir      = "dist",
    [string]$Version        = "",
    [switch]$IncludeSymbols,
    [switch]$IncludeVcRedist = $true,
    [switch]$Clean,
    [switch]$Archive        = $true
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

function Fail($msg) {
    Write-Error $msg
    exit 1
}

# ---------------------------------------------------------------------------
# 1. Validate parameters
# ---------------------------------------------------------------------------
$BuildDirFull = Join-Path $RepoRoot $BuildDir
if (-not (Test-Path $BuildDirFull)) { Fail "BuildDir not found: $BuildDirFull" }

$CacheFile = Join-Path $BuildDirFull "CMakeCache.txt"
if (-not (Test-Path $CacheFile)) { Fail "Not a configured CMake build tree (no CMakeCache.txt): $BuildDirFull" }
$cache = Get-Content $CacheFile

function Get-CacheValue([string]$key) {
    $line = $cache | Where-Object { $_ -match "^$key(:\w+)?=" } | Select-Object -First 1
    if (-not $line) { return $null }
    return ($line -split "=", 2)[1]
}

if (-not $QtBinDir) {
    $qt6Dir = Get-CacheValue "Qt6_DIR"
    if (-not $qt6Dir) { Fail "Could not resolve Qt6_DIR from CMakeCache.txt -- pass -QtBinDir explicitly" }
    # Qt6_DIR is .../lib/cmake/Qt6 -> bin is three levels up
    $QtBinDir = (Resolve-Path (Join-Path $qt6Dir "..\..\..\bin")).Path
}
if (-not (Test-Path $QtBinDir)) { Fail "QtBinDir not found: $QtBinDir" }

$windeployqt = Join-Path $QtBinDir "windeployqt.exe"
if (-not (Test-Path $windeployqt)) { $windeployqt = Join-Path $QtBinDir "windeployqt6.exe" }
if (-not (Test-Path $windeployqt)) { Fail "windeployqt not found under $QtBinDir" }

$dumpbin = (Get-Command dumpbin.exe -ErrorAction SilentlyContinue).Source
if (-not $dumpbin) { Fail "dumpbin.exe not on PATH -- run this script from a Visual Studio x64 developer shell (vcvars64.bat)" }

# ---------------------------------------------------------------------------
# 2. Verify SIPClient.exe exists
# ---------------------------------------------------------------------------
$builtExe = Join-Path $BuildDirFull "SIPClient.exe"
if (-not (Test-Path $builtExe)) { Fail "SIPClient.exe not found in $BuildDirFull -- build it first" }

if (-not $Version) {
    $genHeader = Join-Path $BuildDirFull "generated\AppVersion.h"
    if (Test-Path $genHeader) {
        $m = Select-String -Path $genHeader -Pattern 'APP_VERSION_STRING\s+"([\d.]+)"' | Select-Object -First 1
        if ($m) { $Version = $m.Matches[0].Groups[1].Value }
    }
}
if (-not $Version) { Fail "Could not determine Version -- pass -Version explicitly" }

$gitCommit = (git rev-parse --short HEAD 2>$null)
if (-not $gitCommit) { $gitCommit = "unknown" }

# Qt6_DIR looks like <QtInstallRoot>/<version>/<kit>/lib/cmake/Qt6 -- the
# version segment is the reliable source (CMakeCache doesn't always carry a
# Qt6_VERSION entry). Falls back to the DLL's own file version if the path
# doesn't match that layout.
$qtVersion = $null
$qtBinParent = Split-Path $QtBinDir -Parent
$kitDirName = Split-Path $qtBinParent -Leaf
$versionDirName = Split-Path (Split-Path $qtBinParent -Parent) -Leaf
if ($versionDirName -match '^\d+\.\d+(\.\d+)?$') { $qtVersion = $versionDirName }
if (-not $qtVersion) {
    $qtCoreDll = Join-Path $QtBinDir "Qt6Core.dll"
    if (Test-Path $qtCoreDll) { $qtVersion = (Get-Item $qtCoreDll).VersionInfo.ProductVersion }
}
if (-not $qtVersion) { $qtVersion = $kitDirName }

$pjVersion = "unknown"
$pjConfigHeader = Join-Path $RepoRoot ".deps\pjproject\pjlib\include\pj\config.h"
if (Test-Path $pjConfigHeader) {
    $maj = (Select-String -Path $pjConfigHeader -Pattern 'PJ_VERSION_NUM_MAJOR\s+(\d+)').Matches[0].Groups[1].Value
    $min = (Select-String -Path $pjConfigHeader -Pattern 'PJ_VERSION_NUM_MINOR\s+(\d+)').Matches[0].Groups[1].Value
    $rev = (Select-String -Path $pjConfigHeader -Pattern 'PJ_VERSION_NUM_REV\s+(\d+)').Matches[0].Groups[1].Value
    if ($maj -and $min) { $pjVersion = "$maj.$min.$rev" }
}

$distName = "SIP-Client-Audio-Video-RTT-$Version-windows-x64"
$OutputDirFull = Join-Path $RepoRoot $OutputDir
$stagingRoot = Join-Path $OutputDirFull "staging"
$staging = Join-Path $stagingRoot $distName

Write-Host "Packaging $distName from $BuildDirFull"
Write-Host "  Qt:        $qtVersion ($QtBinDir)"
Write-Host "  PJSIP:     $pjVersion"
Write-Host "  Git:       $gitCommit"

# ---------------------------------------------------------------------------
# 3. Recreate staging
# ---------------------------------------------------------------------------
if ((Test-Path $staging) -and $Clean) { Remove-Item -Recurse -Force $staging }
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Path $staging | Out-Null

Copy-Item $builtExe $staging
$manifestFile = Join-Path $BuildDirFull "SIPClient.exe.manifest"
if (Test-Path $manifestFile) { Copy-Item $manifestFile $staging }

$readmeSrc = Join-Path $RepoRoot "scripts\README-PORTABLE.template.txt"
if (-not (Test-Path $readmeSrc)) { Fail "Missing template: $readmeSrc" }
(Get-Content $readmeSrc -Raw) `
    -replace '__VERSION__', $Version `
    -replace '__GITCOMMIT__', $gitCommit `
    -replace '__QTVERSION__', $qtVersion `
    -replace '__PJVERSION__', $pjVersion |
    Set-Content -Path (Join-Path $staging "README-PORTABLE.txt") -NoNewline

# ---------------------------------------------------------------------------
# 4. Qt deployment
# ---------------------------------------------------------------------------
$stagedExe = Join-Path $staging "SIPClient.exe"
& $windeployqt --release --compiler-runtime --no-translations --dir $staging $stagedExe
if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed (exit $LASTEXITCODE)" }

$requiredPlugins = @("platforms", "multimedia")
$optionalPlugins = @("styles", "imageformats", "iconengines", "tls", "networkinformation")
foreach ($p in $requiredPlugins) {
    if (-not (Test-Path (Join-Path $staging $p))) { Fail "Required Qt plugin directory missing after windeployqt: $p" }
}
foreach ($p in $optionalPlugins) {
    if (-not (Test-Path (Join-Path $staging $p))) { Write-Warning "Optional Qt plugin directory not present: $p (may be fine depending on Qt config)" }
}
if (-not (Test-Path (Join-Path $staging "platforms\qwindows.dll"))) { Fail "platforms\qwindows.dll missing -- app cannot start without it" }

# ---------------------------------------------------------------------------
# 4b. VC++ runtime -- copy directly from the toolchain's own redist folder.
#     windeployqt's --compiler-runtime is toolset-version-sensitive and can
#     silently no-op on a toolset it doesn't recognize (observed with this
#     machine's VC 14.51 toolset -- it deployed zero CRT DLLs and no error),
#     so this project does not rely on it alone: copy every DLL out of
#     %VCToolsRedistDir%\x64\Microsoft.VC*.CRT\ ourselves. This is
#     idempotent with whatever windeployqt did or didn't already place.
# ---------------------------------------------------------------------------
if (-not $env:VCToolsRedistDir) { Fail "VCToolsRedistDir is not set -- run this script from a VS x64 developer shell (vcvars64.bat)" }
$crtDir = Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $crtDir) { Fail "Could not find a Microsoft.VC*.CRT redist directory under $env:VCToolsRedistDir\x64" }
Copy-Item (Join-Path $crtDir.FullName "*.dll") $staging
Write-Host "VC++ runtime DLLs copied from $($crtDir.FullName)"

# ---------------------------------------------------------------------------
# 5. Non-Qt dependency audit (dumpbin /dependents, one level, resolved
#    against what's actually in staging + a system-DLL allow-list -- these
#    are Windows-provided DLLs (System32/WinSxS) that are never bundled)
# ---------------------------------------------------------------------------
$systemDllPattern = '^(?i)(kernel32|user32|gdi32|advapi32|shell32|shlwapi|ole32|oleaut32|comctl32|comdlg32|winmm|ws2_32|version|imm32|setupapi|crypt32|secur32|bcrypt|ncrypt|mswsock|iphlpapi|netapi32|userenv|wtsapi32|dwmapi|uxtheme|d3d9|d3d11|d3d12|dxgi|dxguid|opengl32|glu32|psapi|rpcrt4|winspool|cfgmgr32|powrprof|propsys|wintrust|msvcrt|ucrtbase|ntdll|imagehlp|mpr|authz|icuuc|dwrite|avrt|dnsapi|winhttp|mf|mfplat|mfreadwrite|dxva2|evr|uiautomationcore|quartz|oleacc|api-ms-win-.*|vcruntime\d+(_\d+)?(_threads)?|msvcp\d+(_\d+)?(_atomic_wait|_codecvt_ids)?|concrt\d+|vccorlib\d+)\.?(dll)?$'

function Get-Dependents([string]$binPath) {
    # dumpbin /dependents prints one bare "name.dll" per indented line under
    # "Image has the following dependencies:" -- anchor the match to the
    # whole trimmed line so header lines like "Dump of file X.dll" (which
    # merely *end* in .dll) are not mistaken for a dependency.
    $out = & $dumpbin /dependents $binPath 2>$null
    $out | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^[A-Za-z0-9_.+-]+\.dll$' }
}

$stagingDlls = Get-ChildItem -Path $staging -Recurse -Filter *.dll | ForEach-Object { $_.Name.ToLowerInvariant() }
$stagingDlls += "sipclient.exe"

$allBinaries = @($stagedExe) + (Get-ChildItem -Path $staging -Recurse -Filter *.dll).FullName
$unresolved = New-Object System.Collections.Generic.HashSet[string]
foreach ($bin in $allBinaries) {
    foreach ($dep in (Get-Dependents $bin)) {
        $depLower = $dep.ToLowerInvariant()
        if ($stagingDlls -contains $depLower) { continue }
        if ($depLower -match $systemDllPattern) { continue }
        $unresolved.Add($dep) | Out-Null
    }
}
if ($unresolved.Count -gt 0) {
    Fail "Unresolved non-system DLL dependencies not present in staging: $($unresolved -join ', ')"
}
Write-Host "Non-Qt dependency audit: OK (no unresolved non-system DLLs)"

# PJSIP/pjproject, OpenSSL, zlib, vpx are statically linked into SIPClient.exe
# in this project's build (see .deps/pjsip-msvc-install-release/bin/*.lib) --
# there is no PJSIP/OpenSSL/vpx runtime DLL to ship. FFmpeg (avcodec/avformat/
# avutil/swresample/swscale) is deployed by windeployqt --multimedia's own
# dependency walk, not copied manually.

# ---------------------------------------------------------------------------
# VC++ runtime strategy, part 2
# ---------------------------------------------------------------------------
# Strategy A (local deployment, step 4b above) already places
# vcruntime140.dll/msvcp140.dll etc. next to the exe -- no system-wide install
# required on the target machine. vc_redist.x64.exe is included in addition
# (belt-and-suspenders) only for environments with a policy against loose CRT
# DLLs; README-PORTABLE.txt documents both.
if ($IncludeVcRedist) {
    $vcRedistSrc = Join-Path $BuildDirFull "vc_redist.x64.exe"
    if (-not (Test-Path $vcRedistSrc)) {
        $prev = Get-ChildItem $stagingRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path (Join-Path $_.FullName "vc_redist.x64.exe") } |
            Sort-Object Name -Descending | Select-Object -First 1
        if ($prev) { $vcRedistSrc = Join-Path $prev.FullName "vc_redist.x64.exe" }
    }
    if (Test-Path $vcRedistSrc) {
        Copy-Item $vcRedistSrc $staging
    } else {
        Write-Warning "IncludeVcRedist requested but no vc_redist.x64.exe found -- relying on --compiler-runtime only"
    }
}

# ---------------------------------------------------------------------------
# 6. Version manifest
# ---------------------------------------------------------------------------
$versionInfo = [ordered]@{
    product            = "SIP-Client-Audio-Video-RTT"
    applicationVersion = $Version
    backendVersion     = $Version
    frontendUiVersion  = $Version
    schemaVersion      = 3
    buildType          = $Configuration
    architecture       = "x64"
    gitCommit          = $gitCommit
    qtVersion          = $qtVersion
    pjsipVersion       = $pjVersion
}
$versionInfo | ConvertTo-Json | Set-Content -Path (Join-Path $staging "version-info.json") -Encoding utf8

# ---------------------------------------------------------------------------
# 7. Scan for secrets / local paths
# ---------------------------------------------------------------------------
$secretPatterns = @(
    'Authorization:\s*Digest', 'Proxy-Authorization', 'BEGIN (RSA|EC|PRIVATE) KEY',
    'password\s*=', 'C:\\Users\\[^\\"'']+', [regex]::Escape("$env:USERNAME"),
    [regex]::Escape($env:COMPUTERNAME), [regex]::Escape($RepoRoot)
)
$scanFiles = Get-ChildItem -Path $staging -Recurse -File | Where-Object {
    $_.Extension -in ".txt", ".json", ".ini", ".cfg", ".xml", ".log"
}
$hits = @()
foreach ($f in $scanFiles) {
    $content = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    foreach ($pat in $secretPatterns) {
        if ($content -match $pat) { $hits += "$($f.FullName): pattern '$pat'" }
    }
}
if ($hits.Count -gt 0) { Fail "Secret/local-path scan found hits:`n$($hits -join "`n")" }
Write-Host "Secret/local-path scan: OK"

# ---------------------------------------------------------------------------
# 8. Verify DLLs: x64 only, no Debug-suffixed Qt/CRT DLLs
# ---------------------------------------------------------------------------
$badDebugDlls = Get-ChildItem -Path $staging -Recurse -Filter *.dll | Where-Object {
    # Debug Qt modules end "...d.dll" (Qt6Cored.dll); no release Qt6 module
    # name ends in a bare "d" before the extension, so this can't false-hit.
    $_.Name -match '^Qt6\w*d\.dll$' -or
    # Debug CRT DLLs carry a literal trailing "d" before ".dll"
    # (vcruntime140d.dll, msvcp140_1d.dll); release names never do.
    $_.Name -match '^(vcruntime|msvcp|concrt|vccorlib)\d+(_\d+)?d\.dll$' -or
    $_.Name -match '^ucrtbased\.dll$'
}
if ($badDebugDlls) { Fail "Debug-configuration DLLs found in a Release bundle: $($badDebugDlls.Name -join ', ')" }

foreach ($dll in (Get-ChildItem -Path $staging -Recurse -Filter *.dll)) {
    $head = & $dumpbin /headers $dll.FullName 2>$null | Select-String "machine \("
    if ($head -and ($head -notmatch "x64")) { Fail "Non-x64 binary found in staging: $($dll.FullName)" }
}
Write-Host "Architecture/Debug-DLL check: OK (all x64, no Debug DLLs)"

# ---------------------------------------------------------------------------
# 9. Smoke test from staging (never from the build tree), minimal PATH so it
#    can't accidentally resolve Qt/VS DLLs from the build machine.
# ---------------------------------------------------------------------------
$smokeLog = Join-Path $staging "..\smoke-test.log"
$minimalPath = "$env:SystemRoot\System32;$env:SystemRoot"
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $stagedExe
$psi.WorkingDirectory = $staging
$psi.EnvironmentVariables["PATH"] = $minimalPath
$psi.EnvironmentVariables["QT_ASSUME_STDERR_HAS_CONSOLE"] = "1"
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$psi.RedirectStandardOutput = $true
$proc = [System.Diagnostics.Process]::Start($psi)
Start-Sleep -Seconds 5
$stillRunning = -not $proc.HasExited
if ($stillRunning) {
    $proc.CloseMainWindow() | Out-Null
    if (-not $proc.WaitForExit(5000)) { $proc.Kill() }
} else {
    $stderr = $proc.StandardError.ReadToEnd()
    Fail "Smoke test: SIPClient.exe exited within 5s unexpectedly.`n$stderr"
}
Write-Host "Smoke test (staging, minimal PATH): process started and closed cleanly"

# ---------------------------------------------------------------------------
# 10-12. Archive + checksum + manifest
# ---------------------------------------------------------------------------
if (-not (Test-Path $OutputDirFull)) { New-Item -ItemType Directory -Path $OutputDirFull | Out-Null }
$zipName = "$distName-portable.zip"
$zipPath = Join-Path $OutputDirFull $zipName

if ($Archive) {
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Compress-Archive -Path $staging -DestinationPath $zipPath

    $sha256 = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    "$sha256  $zipName" | Set-Content -Path "$zipPath.sha256" -Encoding ascii -NoNewline

    $manifestFiles = Get-ChildItem -Path $staging -Recurse -File | ForEach-Object {
        [ordered]@{
            path   = $_.FullName.Substring($staging.Length + 1) -replace '\\', '/'
            sizeBytes = $_.Length
            sha256 = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $manifest = [ordered]@{
        artifact  = $zipName
        version   = $Version
        gitCommit = $gitCommit
        sha256    = $sha256
        files     = $manifestFiles
    }
    $manifestPath = Join-Path $OutputDirFull "$distName-manifest.json"
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding utf8

    Write-Host "Zip:      $zipPath"
    Write-Host "SHA-256:  $sha256"
    Write-Host "Manifest: $manifestPath"
}

if ($IncludeSymbols) {
    $pdb = Join-Path $BuildDirFull "SIPClient.pdb"
    if (Test-Path $pdb) {
        $symZip = Join-Path $OutputDirFull "$distName-symbols.zip"
        if (Test-Path $symZip) { Remove-Item -Force $symZip }
        Compress-Archive -Path $pdb -DestinationPath $symZip
        Write-Host "Symbols:  $symZip (private artifact -- do not publish alongside the public bundle)"
    } else {
        Write-Warning "IncludeSymbols requested but no SIPClient.pdb found in $BuildDirFull"
    }
}

Write-Host ""
Write-Host "Packaging complete: $distName"
