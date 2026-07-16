# Agent Result — Task W110: Full Project Code Review, Architecture Audit and Bug Discovery

Full 57-item report per the task's requirements. See
[docs/reviews/](../reviews/) for the detailed supporting documents
referenced throughout.

1. **Branch**: `audit/w110-full-project-code-review`.
2. **Branch de pornire**: `fix/w109a-rtt-renegotiation-and-rtp-port-collision`
   (most recent branch in the chain; no separate `W109` branch existed).
3. **Commit de pornire**: `05fb50c` ("fix(rtt): configurable RTP port range
   and separate RTT accept/reject flows (W109A)").
4. **Versiune**: no formal version tag on this branch; last shipped release
   per `docs/release-notes.md` is v1.4.0 (2026-07-05), unaffected — this
   task did not touch release packaging.
5. **Schema JSON**: `InteropTraceExporter::kSchemaVersion = 3` (confirmed
   current in code; two stale doc references to `2` were found and fixed —
   see finding W110-F019).
6. **Build Debug**: `ENABLE_PJSIP=ON`, `BUILD_TESTS=ON`, MSVC 19.51 via
   NMake — 0 errors, 0 warnings, both before and after this task's fixes.
7. **Build Release**: `ENABLE_PJSIP=ON`, `BUILD_TESTS=ON` — built clean;
   surfaced one real MSVC warning (C4804, Qt6 `QHash::remove()` returns
   `bool` not a count) not visible in the Debug build, fixed as W110-F021.
8. **Teste baseline**: CTest (Debug/PJSIP) — **77/77 passed**, 0 failed,
   12.32s, before any code changes (`build/ctest_baseline.log`).
9. **Warnings**: 0 in Debug; 1 in Release (fixed, see item 7).
10. **Arhitectura proiectului**: documented in full in
    [reviews/W110-architecture-map.md](../reviews/W110-architecture-map.md)
    — 10 top-level `src/` modules, ownership model for every key class,
    threading model (single GUI thread + pjsua2's internal worker thread,
    confirmed no other `QThread`/`std::thread` exists), and 22 flow traces
    (startup through shutdown).
11. **Module analizate**: app, core, emergency, etsi, gui, media, msrp,
    rtt, security, sip — all 10 top-level modules, plus build system
    (CMakeLists.txt, tests/CMakeLists.txt) and documentation.
12. **Lifecycle**: reviewed for SipManager, SipCall, SipAccount,
    MsrpSession, MsrpTransport, MsrpRelayClient,
    MsrpCallPreparationController, file-transfer classes, RttSession, GUI
    panel lifecycle, application shutdown. One High finding (W110-F001,
    MSRP relay allocation refresh orphaning — deferred to W112) and one Low
    finding (W110-F017, timer-connection leak — deferred to W112).
    Everywhere else verified sound (consistent `QPointer` guards,
    disconnect-before-destroy ordering).
13. **Ownership**: documented per-class in the architecture map; no
    ambiguous-ownership or double-delete pattern found.
14. **Threading**: no `QThread`/`moveToThread`/`std::thread`/
    `QtConcurrent` anywhere in `src/` — single GUI-thread app plus
    pjsua2's own internal worker thread(s). Confirmed (not assumed) that
    every PJSIP callback marshals to the Qt thread via `QPointer` +
    `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` before touching
    Qt state.
15. **UI thread**: no blocking calls (`waitForConnected`/
    `waitForReadyRead`/synchronous network I/O) found on the GUI thread;
    MSRP/XCAP are fully async via Qt socket/network signals.
16. **SIP**: reviewed REGISTER, INVITE in/out, provisional responses,
    CANCEL/BYE/ACK, re-INVITE, hold/resume, transport selection. One High
    finding, **fixed**: `Held → Failed` was not a legal transition,
    leaving a dialog that died while on hold permanently stuck
    (W110-F002). Double-hangup and post-terminated `getInfo()` were
    already correctly guarded.
17. **SDP**: reviewed m=audio/video/text/message construction and
    renegotiation. One Medium finding: initial-INVITE RTT answer bypasses
    the profile's RTT setting (W110-F006, deferred to W111).
18. **Audio**: no lifecycle/device bugs found; RTP port isolation between
    same-host instances (Task W109A) reconfirmed correct.
19. **Video**: no lifecycle/device bugs found in the areas reviewed.
20. **RTT**: one Medium finding (W110-F006, above); Task W109A's
    renegotiation-loop and RTP-collision fixes reconfirmed intact (full
    CTest pass including the extended `test_rtt_session.cpp`).
21. **SIP MESSAGE**: Message-ID dedup and Content-Length/byte-counting
    verified correct; no duplicate-send risk found between SIP MESSAGE and
    MSRP.
22. **CPIM**: one Medium finding — bare LF instead of RFC 3862 CRLF
    (W110-F010, deferred to W114, needs live-interop retest).
23. **IMDN**: one Medium finding — `correlateDelivery` can regress
    Displayed→Delivered on out-of-order reports (W110-F009, deferred to
    W114).
24. **is-composing**: timer cleanup verified correct at every exit point
    (destructor, send, and all three enable/disable toggles).
25. **Presence**: no High/Medium findings; resubscribe backoff correctly
    bounded and cancelled on unregister.
26. **XCAP**: no High/Medium findings; W104's path-traversal fix
    reconfirmed applied uniformly via `XcapModels::buildUri`.
27. **MSRP direct**: no High/Medium findings beyond the relay-specific
    ones; W105/W106 connection-hijack protections reconfirmed correct.
28. **MSRP relay**: W110-F001 (High, allocation refresh orphaning) and
    W110-F017 (Low, timer leak) — both deferred to W112 as an
    architectural fix. AUTH/digest handling and retry-bounding verified
    sound.
29. **File transfer**: one High finding — negotiated hash never verified
    against received bytes (W110-F003, deferred to W113); one Low finding
    — reserved Windows filenames not screened (W110-F016, deferred to
    W113). Path traversal already prevented.
30. **Parsere**: single implementation each of CPIM/IMDN/is-composing/SDP
    parsers — no duplicate/divergent parser found.
31. **Binary safety**: deflate/gzip decompression-bomb protection,
    MSRP frame/chunk bounds-checking, and embedded-NUL/byte-counting
    handling all verified sound. One Low finding: `ImdnParser`/
    `IsComposingParser` lack `PidfParser`'s input-size cap, not
    independently exploitable (W110-F015, deferred to W114).
32. **Config**: RTP port range / config-dir isolation (Task W109A)
    reconfirmed correctly wired everywhere needed; credential storage
    verified to never touch plaintext ini. One Medium finding: dead
    "Connection" Settings fields never applied (W110-F011, deferred to
    W115).
33. **UI**: connect()-call audit found no true duplicate signal/slot
    registrations. One High finding, **fixed**: an async MSRP-relay
    call-preparation failure had no user-visible error (W110-F004), now
    reusing the `callFailed` signal/UI wiring from the immediately
    preceding commit. A full widget-by-widget manual click-through was not
    performed at this depth (documented limitation, candidate for W115).
34. **Diagnostics**: credential/Authorization/digest redaction verified
    sound across raw SIP trace, MSRP diagnostics, and XCAP URL logging.
35. **Export**: bounded diagnostics-store retention confirmed
    (`kMaxRetainedEntries` eviction); no unbounded raw-payload capture
    found.
36. **Comparator**: `test_trace_comparator` reviewed as part of the test
    audit — no false-PASS behavior found.
37. **Tests audit**: see
    [reviews/W110-test-coverage-gaps.md](../reviews/W110-test-coverage-gaps.md).
    One Medium finding, **fixed**: a no-op `QVERIFY(true)` test
    (W110-F012). One High structural finding, deferred to W115: PJSIP
    real-backend code paths are `QSKIP`'d in CTest across 7 files, and
    `live_*_probe` executables aren't registered as CTest targets
    (W110-F005) — the rest of the suite is unusually disciplined (exact
    wire-byte/RFC-vector assertions, real TCP harnesses, no padding
    findings needed).
38. **Static analysis**: no dedicated clang-tidy/cppcheck run performed
    (would require new toolchain install, itself a scope-creep risk); used
    the Release build's own MSVC warnings as the static-analysis signal,
    finding and fixing one real issue (W110-F021).
39. **Security review**: see
    [reviews/W110-security-review.md](../reviews/W110-security-review.md)
    for the full threat model. No Critical-severity issue found.
40. **Documentation drift**: `docs/project-status.md`'s active-branch
    pointer was stale (W110-F014, **fixed**); `docs/windows-trace-json-export.md`
    had stale `schemaVersion` references (W110-F019, **fixed**); the
    W109A result's own "79/79" CTest arithmetic didn't match the "out of
    77" the run it was based on actually reported — corrected to 77/77 (75
    pre-existing + 2 new) as part of this task's project-status.md update.
    `CMakeUserPresets.json` is tracked in git with a machine-specific path
    (W110-F013, left for owner decision, not a code bug).
41. **Număr total findings**: 21 (plus one informational/non-bug item,
    W110-F020, excluded from the count).
42. **Critical**: 0.
43. **High**: 5 (W110-F001 through F005).
44. **Medium**: 9 (W110-F006 through F014).
45. **Low**: 7 (W110-F015 through F022, minus the informational F020).
46. **Findings remediate**: 7 — W110-F002, F004, F012, F014, F019, F021, F022.
47. **Findings rămase**: 14, all with acceptance criteria in
    [reviews/W110-remediation-roadmap.md](../reviews/W110-remediation-roadmap.md)
    (tasks W111–W115) or explicitly left for owner decision (W110-F013).
48. **Teste adăugate**: 1 new test
    (`test_call_state_machine.cpp::heldToFailedRequiresDisconnectingReroute`);
    1 existing no-op test replaced with a real assertion
    (`test_registration_state_machine.cpp`).
49. **Regresii**: none. Full CTest suite: 77/77 passed both before and
    after this task's fixes (`build/ctest_baseline.log`,
    `build/ctest_after_fixes.log`).
50. **Risc general**: low for the fixes applied in this task (all small,
    isolated, each covered by a passing regression test or documented
    manual verification); the 14 deferred findings carry the risk profile
    described per-task in the roadmap (mostly Medium — MSRP relay
    lifecycle, file-transfer integrity, and PJSIP test coverage are the
    ones worth prioritizing first).
51. **Production readiness assessment**: no Critical defects found; the
    codebase shows strong existing security/correctness discipline
    (redaction, bounds-checking, path-traversal and decompression-bomb
    protections were consistently already in place and verified, not
    merely assumed). The two High-severity bugs fixed here were real,
    user-visible correctness issues now resolved. The three remaining
    High-severity items are real but architectural — each has its own
    follow-up task rather than being rushed into this review.
52. **Roadmap remediere**: 5 tasks (W111 RTT/answer consent hardening,
    W112 MSRP relay allocation lifecycle, W113 file-transfer
    integrity/MSRP cleanup, W114 messaging consistency, W115 PJSIP
    real-backend test coverage + dead-UI cleanup) — full scope,
    dependencies, and acceptance criteria in
    [reviews/W110-remediation-roadmap.md](../reviews/W110-remediation-roadmap.md).
53. **Commituri**: fix/doc commits on `audit/w110-full-project-code-review`
    for the six applied fixes plus this review's documentation additions
    and `project-status.md` update — see `git log --oneline` on this
    branch for the exact list.
54. **Git status**: clean at the time of the final commit (verified via
    `git status` before push).
55. **Push status**: pushed via
    `git push -u origin audit/w110-full-project-code-review`.
56. **Confirmare fără merge**: no merge into `main` (doesn't exist in this
    repo) or `release` (doesn't exist as a branch; only historical
    `release/vX.Y.Z` tags-as-branches, untouched) was performed.
57. **Confirmare privind pjproject**: pjproject sources were not modified
    anywhere in this task.

## Final confirmation block

- Branch `audit/w110-full-project-code-review` has been pushed.
- No merge into `main` was performed.
- No merge into `release` was performed.
- The review covered the entire project — all 10 `src/` modules, the
  build system, the full test suite, and documentation — not only the
  most recent (W109A) task's changes.
- Every finding is supported by evidence from code, tests, or reproducible
  behavior; none are speculative.
- No interoperability was declared without a real test backing it (where
  a fix touches wire format — W110-F010 — a live-interop retest is
  explicitly listed as a prerequisite for calling that follow-up task done).
- Large fixes not implemented in this task were separated into dedicated
  follow-up tasks (W111–W115) with their own branches, risk assessment,
  and acceptance criteria.
- pjproject sources were not modified.
- No credential or other sensitive information was introduced into the
  repository by this task.
