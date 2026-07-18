# W113G Result — macOS ARM64 Bundle and Cross-Platform Test

## Outcome

W113G is complete for the infrastructure that existed in this session. A
native Apple Silicon Debug and Release application was compiled with the real
PJSIP backend, a relocatable `SIP Client.app` was deployed and audited, all 82
CTest targets passed in both configurations, and ad-hoc-signed ZIP and DMG
artifacts were generated. Windows, clean-machine, Developer ID/notarization,
live SIP/media, and Windows↔macOS interoperability results are not claimed.

## Source control and version

| Item | Result |
|---|---|
| Required base | PASS — `fix/w113f-simplify-client-messaging-and-disable-lmpe` at `8fa1fb0df28b074a2311d3ef6fd6eb984ab5ff6e` |
| Feature branch | PASS — `feature/w113g-macos-arm64-bundle`, created exactly from that commit |
| Implementation commit | `fb45e128f4ec21a516eeefa6cf59554c4ce65570` |
| Application/backend/frontend version | PASS — all `1.6.8` from one CMake `PROJECT_VERSION` |
| macOS bundle build number | `168` |
| pjproject source changes | PASS — none; official 2.17 sources were built privately outside the repository |
| Merge | NOT RUN — prohibited by the task |

## Build host and toolchain

- Host: Apple M2 Pro MacBook Pro, 10 CPU cores, 16 GB RAM, native ARM64.
- OS: macOS 26.5.2 (25F84); deployment target: macOS 13.0.
- Apple Clang: 21.0.0; CMake: 4.4.0-rc2; Ninja: 1.13.0.
- Full Xcode was unavailable; Apple Command Line Tools were active. Therefore
  Xcode-project validation was NOT RUN, while Ninja/Clang builds were real.
- Qt: official Qt 6.11.1 macOS Universal 2 package, executed natively and
  thinned only in the staged bundle to ARM64.
- PJSIP: official 2.17 GNU configure/make build, static ARM64 archives, with
  `PJMEDIA_HAS_VIDEO=1`, CoreAudio, AVFoundation, Metal, VideoToolbox, libyuv,
  WebRTC AEC, and OpenSSL. The upstream experimental PJSIP CMake path was
  rejected because its Darwin TLS implementation is an explicit upstream TODO.
- OpenSSL: official 3.5.1, private no-shared ARM64 build.
- Signing identities: none installed; notarization profile: none.

No serial number, hardware UUID, credential, SIP account, or private path is
stored in the deliverable metadata or this report.

## Implementation and audit

- CMake now creates `SIP Client.app`, uses configurable Qt/PJSIP/OpenSSL roots,
  limits Windows GDI and `.lib` assumptions to Windows, and links Security plus
  the native PJSIP frameworks on macOS.
- `Info.plist` has identifier `org.sipclient.multimedia`, executable/name,
  version 1.6.8/build 168, APPL type, macOS 13.0 minimum, and exact microphone
  and camera usage descriptions.
- `MacKeychainBackend` uses modern Security-framework `SecItem` generic-password
  APIs. It never logs password values or persists them through QSettings.
- `--bundle-smoke-test` loads the packaged executable and linked libraries
  without opening UI, profiles, network, camera, or microphone.
- `scripts/package-macos.sh` configures/builds/tests, runs `macdeployqt`, thins
  Universal 2 deployment copies, audits every Mach-O and dependency/RPATH,
  scans the executable for local identity/path leakage, signs nested code then
  the app, verifies the signature, tests a relocated copy, and emits ZIP,
  SHA-256, manifest, and optional DMG/symbol artifacts.
- Deployed audit: 28 Mach-O files, all thin ARM64; no x86_64 slice; no absolute
  non-system dependency; no development RPATH; PJSIP/OpenSSL are static.
- Bundle signature: PASS for ad-hoc `codesign --verify --deep --strict`;
  identifier `org.sipclient.multimedia`, TeamIdentifier unset as expected.
- Gatekeeper assessment: REJECTED as expected for an ad-hoc, unnotarized app.
  This is not represented as a signing/notarization PASS.

## Automated and local macOS validation

| Validation | Debug | Release |
|---|---:|---:|
| Configure/build, ARM64, real PJSIP | PASS | PASS |
| CTest | 82/82 PASS | 82/82 PASS |
| Bundle configuration test | PASS | PASS |
| Original bundle smoke test | PASS | PASS |
| Deployed/relocated smoke test | N/A | PASS |

The first sandboxed Debug CTest attempt had five failures because localhost
binds and first-use CoreAudio initialization were restricted. The exact five
tests passed when rerun with normal host access, followed by a clean complete
82/82 Debug run. The final Release packaging run also completed 82/82.

PJSIP initialized CoreAudio under the native test process and enumerated its
colorbar video devices. The host/test context did not provide a usable physical
camera and the private PJSIP build had no VPX/OpenH264/FFmpeg codec, so no video
codec appeared in PJSIP enumeration. This is recorded as a limitation, not a
video-call PASS.

## Artifacts

Both artifacts were generated from implementation commit
`fb45e128f4ec21a516eeefa6cf59554c4ce65570` at `2026-07-18T22:37:41Z`.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `SIP-Client-Audio-Video-RTT-1.6.8-macos-arm64.zip` | 24,170,871 | `434edba98479cbc067f7e186989bddc7c98cde3aea979cb2cdeb56ebc86f29ca` |
| `SIP-Client-Audio-Video-RTT-1.6.8-macos-arm64.dmg` | 26,967,549 | `013dcef5ad1963c73982c4ff18bdc747970ae922b28d32568464cc7047681de2` |

The checked output also contains the ZIP `.sha256` sidecar and JSON manifest.
A dSYM was not generated by this Release configuration, so a symbols archive
was NOT RUN / not emitted rather than fabricated.

## Manual and external test truth table

| Test | Status | Reason/evidence |
|---|---|---|
| Launch packaged app via smoke entrypoint | PASS | Original and relocated app both print versioned PASS and exit 0 |
| Interactive GUI launch/click-through | NOT RUN | No GUI automation was used; smoke path intentionally opens no UI |
| Microphone permission prompt/deny/re-enable | NOT RUN | Would alter host privacy state; no disposable interactive session |
| Camera permission prompt/LED/preview | NOT RUN | No usable physical camera in test context |
| Real Keychain save/load/delete | NOT RUN | Automated tests inject memory backend; no disposable SIP credential was authorized |
| Real SIP REGISTER | BLOCKED | No SIP server/accounts supplied |
| Real audio call and device switching | BLOCKED | No registered peer/accounts |
| Real video call | BLOCKED | No peer/camera and PJSIP video codec absent |
| RTT with audio/video | BLOCKED | No live peer/accounts |
| SIP MESSAGE/MSRP/IMDN/is-composing live interop | BLOCKED | No live peer/accounts |
| LMPE inactive behavior | PASS (automated/structural) | Existing unconfirmed-codec test passes; W113G added no LMPE activation |
| Developer ID signing | BLOCKED | No Developer ID Application identity installed |
| Apple notarization/stapling | BLOCKED | No Developer ID identity or notarytool profile |
| Clean second Apple Silicon Mac | BLOCKED | Only one Mac available |
| Windows Debug/Release build | BLOCKED / NOT RUN | No Windows host/VM and no configured repository CI workflow |
| Windows regression and portable bundle | BLOCKED / NOT RUN | Same Windows infrastructure absence; Windows-only sources remain guarded/preserved |
| Windows → macOS and macOS → Windows calls | BLOCKED / NOT RUN | No Windows machine, SIP server/accounts, or reference endpoint |

No absent external test is inferred from compilation or unit tests. The exact
future matrix is in
[windows-macos-interoperability-test.md](../windows-macos-interoperability-test.md).

## Remaining release blockers

Before public distribution, obtain a Developer ID identity/notarization
profile, run the signing/notarization path, validate ZIP/DMG on a clean second
Apple Silicon Mac, build/test Windows on a real Windows toolchain, provision a
SIP server and accounts, and execute the bidirectional interoperability matrix.
For PJSIP video calls, add a supported ARM64 video codec dependency without
vendoring or editing pjproject and then perform live camera/render validation.
