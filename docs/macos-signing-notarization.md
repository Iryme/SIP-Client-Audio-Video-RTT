# macOS Signing and Notarization

`scripts/package-macos.sh` supports two signing modes:

- `--signing-identity -`: real ad-hoc signing for local/test distribution.
- `--signing-identity "Developer ID Application: ..."`: Developer ID signing.

Nested frameworks/plugins are signed before the outer app and then verified
with `codesign --verify --deep --strict`. Camera/microphone access needs no
special sandbox entitlement for this non-App-Store bundle; the Info.plist
usage strings are mandatory.

For notarization, first store credentials using `xcrun notarytool
store-credentials`, then pass `--notarization-profile PROFILE` together with a
Developer ID identity. The script submits the ZIP, waits, staples and validates
the app, and recreates final containers from the stapled bundle.

W113G's local artifact is ad-hoc signed. Developer ID signing and notarization
were not run because no signing identity or notary profile was available.
Gatekeeper rejection of an ad-hoc build is therefore expected and is not a
notarization PASS.

