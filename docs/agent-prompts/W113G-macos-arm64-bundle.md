# W113G — macOS ARM64 Build, Bundle, Packaging and Cross-Platform Test

Source prompt: `W113G-macOS-ARM64-build-and-cross-platform-test-prompt.txt`.

Implement version 1.6.8 on branch `feature/w113g-macos-arm64-bundle`, based
exactly on W113F. Audit and isolate Windows assumptions; add native Apple
Silicon Debug/Release builds, a correct `SIP Client.app`, real macOS platform
adapters, relocatable Qt deployment, architecture/dependency/RPATH/security
audits, signing/notarization-ready packaging, ZIP/SHA/manifest and optional
DMG/symbols. Run real tests where infrastructure exists and report Windows,
clean-machine, signing, notarization, live SIP/media/RTT/messaging, and
Windows↔macOS results accurately as PASS/FAIL/BLOCKED/NOT RUN. Do not edit
pjproject sources, invent results, merge the branch, or expose secrets.

Required documentation includes the macOS build/deployment/platform/media/
signing guides, interoperability matrix, release/status/version updates, and
a detailed result report.

