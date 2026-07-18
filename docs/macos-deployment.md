# macOS Deployment

The distribution unit is `SIP Client.app`. It contains the executable,
deployed Qt frameworks and plugins, `Info.plist`, and
`Resources/version-info.json`. PJSIP and OpenSSL are statically linked, so no
PJSIP/OpenSSL dylib belongs in the bundle.

The bundle identifier defaults to `org.sipclient.multimedia`; minimum macOS
is 13.0. `Info.plist` carries version 1.6.8, build 168, and microphone/camera
usage descriptions. Runtime lookup is restricted to bundle-relative
`@executable_path`, `@loader_path`, and `@rpath` locations plus Apple system
libraries.

Copy the ZIP or DMG to another Apple Silicon Mac, place the app in
`/Applications`, and run it. For the W113G ad-hoc artifact, Gatekeeper may
require an explicit user override because it is not Developer-ID signed or
notarized. Production distribution must follow
[macos-signing-notarization.md](macos-signing-notarization.md).

Validation commands:

```bash
lipo -info "SIP Client.app/Contents/MacOS/SIP Client"
otool -L "SIP Client.app/Contents/MacOS/SIP Client"
codesign --verify --deep --strict --verbose=4 "SIP Client.app"
"SIP Client.app/Contents/MacOS/SIP Client" --bundle-smoke-test
```

Do not publish a bundle containing local build paths, x86_64 slices, Debug Qt
libraries, credentials, private keys, or SIP account data.

