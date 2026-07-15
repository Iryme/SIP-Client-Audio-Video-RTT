# W110 Test Coverage Gaps — SIP-Client-Audio-Video-RTT

Based on a sampled read of 15+ of the 82 test files (spanning MSRP, SIP
core, RTT, emergency, diagnostics, messaging) plus a full read of
`tests/CMakeLists.txt` (1954 lines). Overall assessment: **this is an
unusually disciplined test suite** — most tests assert exact wire
bytes/strings/final state using RFC test vectors, real `QTcpServer`-backed
transports instead of mocks, and even self-documenting comments explaining
why a given fake/harness is trustworthy. The gaps below are the genuine
exceptions, not padding.

## Module coverage-gap table

| Module | Tested well | Notable gap | Priority |
|---|---|---|---|
| MSRP (relay/frame/digest) | Real TCP fake-relay harness, RFC 2617 worked-vector digest math, frame parser/serializer round-trips | Relay-to-live-call wiring (W108) has no wire-level test of the actual `makeCall()`-integration path | Medium |
| SIP core (SipManager/Registration/Profile) | State machine transitions, password-redaction-in-diagnostics test | Real-backend (`HAVE_PJSIP`) registration/call logic is `QSKIP`'d in 7 test files; only exercised by non-CTest `live_*_probe` executables (W110-F005) | High |
| RTT | Exhaustive TX/RX delta algorithm and state-transition coverage | Real PJSIP text-stream wiring untested (same stub/PJSIP split as above) | Medium |
| Emergency (NG112) | Full end-to-end chain test with header assertions, full state-machine coverage | Live SIP integration explicitly and correctly documented as never captured in this environment | Low |
| Presence / XCAP | Precise policy/backoff tests, URL-encoding security-regression tests (path traversal, query/fragment injection) | Presence PUBLISH is experimental and untested — consistent with its documented status | Low |
| Diagnostics (bundle/timeline/snapshot) | Reads back real ZIP/folder bytes, JSON structure, CSV header, redaction of a real secret value | One self-documented run-order dependency (`test_diagnostics_bundle.cpp`, W110-F018) | Low |
| Messaging (CPIM/IMDN/is-composing) | RFC-vector-style exact byte/string assertions | `correlateDelivery` precedence gap (W110-F009) has no regression test guarding against it | Medium |
| File transfer | Selector/receiver/hash-verification unit tests all pass | `verifyHash()` is unit-tested but never called from production code (W110-F003) — the test proves the function works, not that the feature works | High |

## Specific findings

1. **Placebo test** (`tests/test_registration_state_machine.cpp`,
   `passwordNeverAppearsInStateTransitionLogs`) — collected log messages but
   asserted nothing (`QVERIFY(true)`). **Fixed in this task** — see
   W110-F012 in [W110-findings.md](W110-findings.md).

2. **Coverage split masks PJSIP-mode regressions** — `test_sip_manager.cpp`,
   `test_registration_state_machine.cpp`, `test_registration_retry.cpp`,
   `test_registration_expiry.cpp`, `test_profile_switch.cpp`,
   `test_audio_media.cpp`, `test_video_media.cpp` all `QSKIP` their
   meaningful bodies under `#ifdef HAVE_PJSIP`. Combined with `live_*_probe`
   never being an `add_test` target, the real pjsua2 registration/call
   control glue has no CTest regression net. See W110-F005 — proposed as
   its own follow-up task (W115) rather than attempted here, since it
   requires either a mockable pjsua2 seam or promoting live-probe
   assertions into CTest-safe tests.

3. **Test-order dependency** (`test_diagnostics_bundle.cpp`,
   `fallbackWhenSipTraceMissing`) — must run before any other test in the
   same binary touches `SipTraceLogger`; relies on QtTest's
   declaration-order execution. Self-documented in a comment, confined to
   one binary. See W110-F018.

No duplicate tests, no unusually large tolerances/timeouts (largest
explicit timeout found: 5000 ms in `test_msrp_relay_client.cpp`), and no
test with a stub/fake that defeats its own purpose were found — e.g. the
`FakeMsrpRelay` in `test_msrp_relay_client.cpp` uses a real TCP socket and
the real frame parser/serializer; only the relay's protocol responses are
scripted, which is an appropriate, well-reasoned boundary.

## Test count baseline

`tests/CMakeLists.txt` registers 76 unconditional `add_test` entries plus 1
gated behind `ENABLE_PJSIP AND PJSIP_FOUND`
(`test_msrp_sip_media_injector`), totaling **77** when built with PJSIP —
matches the CTest baseline recorded in
[W110-full-project-code-review.md](W110-full-project-code-review.md)
exactly (77/77 passed, both before and after this task's fixes).
