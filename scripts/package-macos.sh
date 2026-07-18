#!/bin/bash
set -euo pipefail

usage() {
    printf '%s\n' \
      'Usage: scripts/package-macos.sh --qt-path PATH [options]' \
      '  --build-dir PATH              Build directory (default: build-macos-arm64-release)' \
      '  --configuration NAME          Debug or Release (default: Release)' \
      '  --architecture ARCH           Must be arm64 for W113G (default: arm64)' \
      '  --deployment-target VERSION   Minimum macOS (default: 13.0)' \
      '  --qt-path PATH                 Native Qt macOS prefix (required)' \
      '  --pjsip-root PATH              PJSIP 2.17 installation prefix' \
      '  --openssl-root PATH            Static OpenSSL installation prefix' \
      '  --output-dir PATH              Artifact directory (default: dist)' \
      '  --version VERSION              Expected application version (default: 1.6.8)' \
      '  --bundle-identifier ID         Bundle identifier' \
      '  --signing-identity ID          - for ad-hoc, or a real Developer ID identity' \
      '  --notarization-profile NAME    Existing notarytool keychain profile' \
      '  --create-dmg                    Also create a DMG' \
      '  --include-symbols               Package a real dSYM if generated' \
      '  --clean                         Remove this script’s exact build/staging outputs first'
}

script_dir="$(cd "$(dirname "$0")" && pwd -P)"
source_dir="$(cd "$script_dir/.." && pwd -P)"
build_dir="$source_dir/build-macos-arm64-release"
configuration="Release"
architecture="arm64"
deployment_target="13.0"
qt_path=""
pjsip_root=""
openssl_root=""
output_dir="$source_dir/dist"
version="1.6.8"
bundle_identifier="org.sipclient.multimedia"
signing_identity="-"
notarization_profile=""
create_dmg=0
include_symbols=0
clean=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) build_dir="$2"; shift 2 ;;
        --configuration) configuration="$2"; shift 2 ;;
        --architecture) architecture="$2"; shift 2 ;;
        --deployment-target) deployment_target="$2"; shift 2 ;;
        --qt-path) qt_path="$2"; shift 2 ;;
        --pjsip-root) pjsip_root="$2"; shift 2 ;;
        --openssl-root) openssl_root="$2"; shift 2 ;;
        --output-dir) output_dir="$2"; shift 2 ;;
        --version) version="$2"; shift 2 ;;
        --bundle-identifier) bundle_identifier="$2"; shift 2 ;;
        --signing-identity) signing_identity="$2"; shift 2 ;;
        --notarization-profile) notarization_profile="$2"; shift 2 ;;
        --create-dmg) create_dmg=1; shift ;;
        --include-symbols) include_symbols=1; shift ;;
        --clean) clean=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

[[ "$(uname -s)" == "Darwin" ]] || { printf 'macOS is required\n' >&2; exit 1; }
[[ "$(uname -m)" == "arm64" ]] || { printf 'Native Apple Silicon execution is required\n' >&2; exit 1; }
[[ "$architecture" == "arm64" ]] || { printf 'W113G supports arm64 only\n' >&2; exit 1; }
[[ "$configuration" == "Debug" || "$configuration" == "Release" ]] || {
    printf 'Configuration must be Debug or Release\n' >&2; exit 1;
}
[[ "$version" == "1.6.8" ]] || { printf 'W113G version must be 1.6.8\n' >&2; exit 1; }
[[ -n "$qt_path" && -d "$qt_path" ]] || { printf 'A valid --qt-path is required\n' >&2; exit 1; }

cmake_bin="$(command -v cmake)"
ninja_bin="$(command -v ninja || true)"
macdeployqt="$qt_path/bin/macdeployqt"
[[ -n "$cmake_bin" ]] || { printf 'cmake is required\n' >&2; exit 1; }
[[ -n "$ninja_bin" ]] || { printf 'ninja is required\n' >&2; exit 1; }
[[ -x "$macdeployqt" ]] || { printf 'macdeployqt not found under Qt prefix\n' >&2; exit 1; }

build_dir="$(mkdir -p "$build_dir" && cd "$build_dir" && pwd -P)"
output_dir="$(mkdir -p "$output_dir" && cd "$output_dir" && pwd -P)"
staging_dir="$build_dir/w113g-package-staging"
bundle="$build_dir/SIP Client.app"
artifact_base="SIP-Client-Audio-Video-RTT-${version}-macos-arm64"
zip_path="$output_dir/${artifact_base}.zip"
manifest_path="$output_dir/${artifact_base}-manifest.json"

if [[ "$clean" -eq 1 ]]; then
    [[ "$build_dir" == "$source_dir" || "$build_dir" == "/" ]] && {
        printf 'Refusing unsafe clean target\n' >&2; exit 1;
    }
    rm -rf "$build_dir"
    mkdir -p "$build_dir"
fi
rm -rf "$staging_dir"
mkdir -p "$staging_dir"

configure_args=(
    -S "$source_dir" -B "$build_dir" -G Ninja
    "-DCMAKE_BUILD_TYPE=$configuration"
    "-DCMAKE_OSX_ARCHITECTURES=$architecture"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=$deployment_target"
    "-DSIPCLIENT_QT_ROOT=$qt_path"
    "-DSIPCLIENT_BUNDLE_IDENTIFIER=$bundle_identifier"
    "-DSIPCLIENT_SIGNING_IDENTITY=$signing_identity"
    -DENABLE_PJSIP=ON
    -DBUILD_TESTS=ON
)
if [[ -n "$pjsip_root" ]]; then
    configure_args+=("-DPJSIP_ROOT=$pjsip_root")
fi
if [[ -n "$openssl_root" ]]; then
    configure_args+=("-DOPENSSL_ROOT_DIR=$openssl_root" "-DOPENSSL_USE_STATIC_LIBS=TRUE")
fi
"$cmake_bin" "${configure_args[@]}"
"$cmake_bin" --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

[[ -d "$bundle" ]] || { printf 'Expected bundle missing: %s\n' "$bundle" >&2; exit 1; }
"$macdeployqt" "$bundle" -verbose=2

# Qt's official macOS SDK is distributed as Universal 2. The application and
# W113G deliverable are arm64-only, so remove the unused x86_64 slices from the
# deployed copy before signing. The installed Qt SDK itself is not modified.
while IFS= read -r binary; do
    if file "$binary" | grep -q 'Mach-O' && lipo -info "$binary" | grep -q 'x86_64'; then
        lipo "$binary" -thin arm64 -output "$binary.w113g-thin"
        chmod --reference="$binary" "$binary.w113g-thin" 2>/dev/null || chmod 755 "$binary.w113g-thin"
        mv "$binary.w113g-thin" "$binary"
    fi
done < <(find "$bundle/Contents/MacOS" "$bundle/Contents/Frameworks" "$bundle/Contents/PlugIns" -type f)

# CMake and macdeployqt must establish relocatable install names.  Do not hide
# an incorrect build behind opaque install_name_tool rewrites here.
while IFS= read -r binary; do
    if file "$binary" | grep -q 'Mach-O'; then
        arch_info="$(lipo -info "$binary")"
        printf '%s\n' "$arch_info" | grep -q 'arm64' || {
            printf 'Non-arm64 component: %s\n' "$binary" >&2; exit 1;
        }
        if printf '%s\n' "$arch_info" | grep -q 'x86_64'; then
            printf 'Universal/x86_64 component is outside W113G scope: %s\n' "$binary" >&2
            exit 1
        fi
        otool -L "$binary" | tail -n +2 | awk '{print $1}' | while IFS= read -r dependency; do
            case "$dependency" in
                @rpath/*|@loader_path/*|@executable_path/*|/System/*|/usr/lib/*) ;;
                *) printf 'Non-relocatable dependency in %s: %s\n' "$binary" "$dependency" >&2; exit 1 ;;
            esac
        done
    fi
done < <(find "$bundle/Contents/MacOS" "$bundle/Contents/Frameworks" "$bundle/Contents/PlugIns" -type f)

if otool -l "$bundle/Contents/MacOS/SIP Client" \
    | awk '/cmd LC_RPATH/{in_rpath=1; next} in_rpath && /path /{print $2; in_rpath=0}' \
    | grep -E '^(/Users/|/opt/homebrew|/usr/local|.*build-macos)' >/dev/null; then
    printf 'Local/development RPATH found in executable\n' >&2
    exit 1
fi

git_commit="$(git -C "$source_dir" rev-parse HEAD)"
git_branch="$(git -C "$source_dir" branch --show-current)"
build_timestamp="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
qt_version="$($qt_path/bin/qtpaths --qt-version)"
pjsip_version="2.17"
signing_mode="adhoc"
[[ "$signing_identity" != "-" ]] && signing_mode="developer-id"
notarization_status="not-run"
[[ -n "$notarization_profile" ]] && notarization_status="pass"
resources_dir="$bundle/Contents/Resources"
mkdir -p "$resources_dir"
cat > "$resources_dir/version-info.json" <<EOF
{
  "product": "SIP-Client-Audio-Video-RTT",
  "applicationVersion": "$version",
  "backendVersion": "$version",
  "frontendUiVersion": "$version",
  "schemaVersion": "3",
  "buildType": "$configuration",
  "platform": "macOS",
  "architecture": "$architecture",
  "gitBranch": "$git_branch",
  "gitCommit": "$git_commit",
  "buildTimestampUtc": "$build_timestamp",
  "qtVersion": "$qt_version",
  "pjsipVersion": "$pjsip_version",
  "deploymentTarget": "$deployment_target",
  "signingMode": "$signing_mode",
  "notarizationStatus": "$notarization_status"
}
EOF

if strings "$bundle/Contents/MacOS/SIP Client" | grep -E '/Users/[^/]+/|Irymes-MacBook' >/dev/null; then
    printf 'Developer identity or local path found in executable strings\n' >&2
    exit 1
fi

# Sign inner code first, then the outer bundle. Entitlements are intentionally
# omitted: camera and microphone consent is driven by Info.plist usage strings.
while IFS= read -r component; do
    codesign --force --sign "$signing_identity" --timestamp=none "$component"
done < <(find "$bundle/Contents/Frameworks" "$bundle/Contents/PlugIns" -depth \
    \( -name '*.framework' -o -name '*.dylib' -o -name '*.so' \) 2>/dev/null)
codesign --force --sign "$signing_identity" --timestamp=none "$bundle"
codesign --verify --deep --strict --verbose=4 "$bundle"
spctl --assess --type execute --verbose=4 "$bundle" || true

"$bundle/Contents/MacOS/SIP Client" --bundle-smoke-test

relocated_dir="$staging_dir/relocated"
mkdir -p "$relocated_dir"
ditto "$bundle" "$relocated_dir/SIP Client.app"
"$relocated_dir/SIP Client.app/Contents/MacOS/SIP Client" --bundle-smoke-test

rm -f "$zip_path" "$zip_path.sha256" "$manifest_path"
ditto -c -k --sequesterRsrc --keepParent "$bundle" "$zip_path"

symbols_path=""
dsym="$bundle.dSYM"
if [[ "$include_symbols" -eq 1 && -d "$dsym" ]]; then
    symbols_path="$output_dir/${artifact_base}-symbols.zip"
    ditto -c -k --sequesterRsrc --keepParent "$dsym" "$symbols_path"
fi

if [[ -n "$notarization_profile" ]]; then
    [[ "$signing_mode" == "developer-id" ]] || {
        printf 'Notarization requires a Developer ID identity\n' >&2; exit 1;
    }
    xcrun notarytool submit "$zip_path" --keychain-profile "$notarization_profile" --wait
    xcrun stapler staple "$bundle"
    xcrun stapler validate "$bundle"
    # Recreate the final deliverable so it contains the stapled bundle.
    rm -f "$zip_path"
    ditto -c -k --sequesterRsrc --keepParent "$bundle" "$zip_path"
fi

# Create the optional disk image only after any notarization/stapling so every
# final container carries the same bundle state.
dmg_path=""
if [[ "$create_dmg" -eq 1 ]]; then
    dmg_path="$output_dir/${artifact_base}.dmg"
    rm -f "$dmg_path"
    hdiutil create -volname 'SIP Client' -srcfolder "$bundle" -ov -format UDZO "$dmg_path"
fi

zip_sha="$(shasum -a 256 "$zip_path" | awk '{print $1}')"
printf '%s  %s\n' "$zip_sha" "$(basename "$zip_path")" > "$zip_path.sha256"

cat > "$manifest_path" <<EOF
{
  "product": "SIP-Client-Audio-Video-RTT",
  "version": "$version",
  "platform": "macOS",
  "architecture": "$architecture",
  "deploymentTarget": "$deployment_target",
  "gitBranch": "$git_branch",
  "gitCommit": "$git_commit",
  "buildTimestampUtc": "$build_timestamp",
  "qtVersion": "$qt_version",
  "pjsipVersion": "$pjsip_version",
  "bundleIdentifier": "$bundle_identifier",
  "signingMode": "$signing_mode",
  "notarizationStatus": "$notarization_status",
  "zip": "$(basename "$zip_path")",
  "zipSha256": "$zip_sha",
  "dmg": "$(basename "$dmg_path")",
  "symbols": "$(basename "$symbols_path")"
}
EOF

printf 'Bundle: %s\nZIP: %s\nSHA-256: %s\nManifest: %s\n' \
    "$bundle" "$zip_path" "$zip_sha" "$manifest_path"
