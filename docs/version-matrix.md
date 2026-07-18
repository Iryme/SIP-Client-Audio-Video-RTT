# Version Matrix

This file did not exist before Task W113H. It tracks, per application
version, which platforms have had a real (not inferred) build+test pass and
which packaged artifacts exist — not full task history, which lives in
[project-status.md](project-status.md).

| Version | Windows build+test | macOS build+test | Windows bundle | macOS bundle | Live cross-platform test |
|---|---|---|---|---|---|
| 1.6.8 | PASS — Debug+Release, 81/81 CTest (W113H) | PASS — Debug+Release, 82/82 CTest (W113G) | PASS — portable ZIP+SHA-256+manifest generated and audited (W113H) | PASS — ZIP+DMG, ad-hoc signed, audited (W113G) | BLOCKED — no session has had both platforms + a live SIP server simultaneously |
| 1.6.7 | PASS (W113F session) | NOT RUN | Generated in W113F session (superseded, see `dist/`) | NOT RUN | NOT RUN |
| 1.6.6 | PASS (W113E session) | NOT RUN | NOT RUN this version | NOT RUN | NOT RUN |
| 1.6.4 | PASS (W113D session) | NOT RUN | PASS — first portable-bundle task (W113D) | NOT RUN | NOT RUN |

## Notes

- Only 1.6.8 has had both a Windows (W113H) and a macOS (W113G) build/test/
  package pass performed. Every earlier version was Windows-only or had a
  narrower scope; see each version's own task in
  [project-status.md](project-status.md) for exact detail.
- "Live cross-platform test" only ever moves off BLOCKED when one session
  has simultaneous access to a Windows machine, a macOS device, and a live
  SIP server with two accounts — see
  [windows-macos-interoperability-test.md](windows-macos-interoperability-test.md).
- The Windows 1.6.8 PJSIP build has no TLS transport compiled in
  (`PJ_HAS_SSL_SOCK 0`) — see [known-limitations.md](known-limitations.md).
  The macOS 1.6.8 PJSIP build does link OpenSSL per W113G's report, so TLS
  support is currently asymmetric between the two platforms at the same
  application version.
