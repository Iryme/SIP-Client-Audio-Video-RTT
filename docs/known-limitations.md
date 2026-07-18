# Known Limitations

This file did not exist before Task W113H; it consolidates limitations that
were previously scattered across individual task docs and agent-result files.
It records limitations that are true of the current codebase/build
configuration, not task-specific NOT RUN/BLOCKED items (those live in each
task's own agent-result doc and in
[windows-macos-interoperability-test.md](windows-macos-interoperability-test.md)).

## Windows build has no TLS transport in PJSIP

Discovered in W113H: the Windows PJSIP install under `.deps/pjsip-msvc-install[-release]`
was configured without OpenSSL (`OPENSSL_INCLUDE_DIR-NOTFOUND`/`OpenSSL_DIR-NOTFOUND`
at PJSIP's own CMake configure time), so the installed `pj/config.h` and
`pj/compat/os_auto.h` both report `PJ_HAS_SSL_SOCK 0`. SIP over TLS cannot be
used on Windows with this build — only UDP/TCP are available. This is a
pre-existing build-configuration fact (not introduced or changed by W113H),
first documented here because W113H was the first task to inspect the PJSIP
install's SSL configuration directly. A fix requires locating an OpenSSL
install/vcpkg package and re-running PJSIP's own CMake configure with it —
out of scope for a task that must not modify pjproject build inputs without
separate justification.

## macOS PJSIP build has no video codec enumerated

Per [W113G-macos-arm64-bundle-result.md](agent-results/W113G-macos-arm64-bundle-result.md):
the private macOS PJSIP build had no VPX/OpenH264/FFmpeg codec compiled in, so
no video codec appears in PJSIP's codec enumeration on macOS. A Windows↔macOS
video call cannot currently negotiate a common video codec until this is
addressed in a dedicated task (adding a codec dependency without vendoring or
editing pjproject).

## Live Windows↔macOS interoperability has never been executed

Both W113G and W113H validated their own platform's build/test/package
pipeline in isolation. Neither session had simultaneous access to a Windows
machine, a macOS device, and a live SIP server — see
[windows-macos-interoperability-test.md](windows-macos-interoperability-test.md)
for the full scenario list still NOT RUN/BLOCKED.

## Single active call

`SipManager` supports exactly one active call at a time — documented in
[client-messaging-workspace.md](client-messaging-workspace.md). Multiple
simultaneous calls to the same peer are not applicable in the current app.

## No CI configured

This repository has no CI workflow. Every build/test/package verification
recorded in `docs/agent-results/` is a manual, single-machine run performed
during that task's session — there is no automated recheck on subsequent
commits unless a future task re-runs the same steps.
