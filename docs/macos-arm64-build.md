# macOS ARM64 Build

W113G adds a native Apple Silicon build for SIP Client 1.6.8. The supported
baseline is macOS 13.0, Qt 6.4 or newer, CMake 3.21 or newer, Ninja, and an
ARM64 PJSIP 2.17 static installation. Qt, PJSIP, and OpenSSL locations are
cache parameters; no developer-specific path is committed.

```bash
cmake -S . -B build-macos-arm64-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DSIPCLIENT_QT_ROOT=/path/to/Qt/macos \
  -DPJSIP_ROOT=/path/to/pjsip-arm64 \
  -DOPENSSL_ROOT_DIR=/path/to/openssl-arm64 \
  -DOPENSSL_USE_STATIC_LIBS=TRUE \
  -DENABLE_PJSIP=ON -DBUILD_TESTS=ON
cmake --build build-macos-arm64-release --parallel
ctest --test-dir build-macos-arm64-release --output-on-failure
```

PJSIP must be built consistently with `PJ_AUTOCONF=1`, little-endian ARM64,
and `PJMEDIA_HAS_VIDEO=1`. The W113G validation used unmodified official
PJSIP 2.17 sources, configured with `-arch arm64` and a private static
OpenSSL 3.5.1 prefix. The upstream experimental PJSIP CMake build is not used
because its Darwin TLS branch is currently an explicit TODO; the supported
PJSIP GNU configure/make path works.

The resulting bundle is `build-macos-arm64-release/SIP Client.app`.

For a deployable artifact, use the checked-in workflow:

```bash
PATH=/path/to/ninja:$PATH scripts/package-macos.sh \
  --qt-path /path/to/Qt/macos \
  --pjsip-root /path/to/pjsip-arm64 \
  --openssl-root /path/to/openssl-arm64 \
  --signing-identity - --create-dmg
```

The script deploys Qt, removes x86_64 slices from the staged copy, audits
architectures/install names/RPATHs/secrets, signs, smoke-tests from the build
and relocated paths, and writes ZIP, SHA-256, JSON manifest, and optional DMG.

