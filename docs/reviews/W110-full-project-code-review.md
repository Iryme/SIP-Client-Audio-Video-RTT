# W110 — Full Project Code Review, Architecture Audit and Bug Discovery

## Branch / starting point

- New branch: `audit/w110-full-project-code-review`.
- Starting branch: `fix/w109a-rtt-renegotiation-and-rtp-port-collision`
  (the most recent branch in the chain — confirmed via `git branch -a`
  showing no separate `W109` branch, and `git log --oneline --decorate -30`
  showing it as the tip ahead of `feature/w108-relay-live-call-integration`).
- Starting commit: `05fb50c` ("fix(rtt): configurable RTP port range and
  separate RTT accept/reject flows (W109A)").
- No `main` or `release` branch exists in this repo (confirmed via
  `git branch -a`); `release/v1.2.0`, `v1.3.0`, `v1.4.0` exist as tags of
  work, not integration targets — no merge was performed into any of them.

## Baseline (before this task's fixes)

| Configuration | Result |
|---|---|
| Debug, `ENABLE_PJSIP=ON`, `BUILD_TESTS=ON` (MSVC 19.51, NMake) | Build: 0 errors, 0 warnings. |
| CTest (Debug/PJSIP) | **77/77 passed**, 0 failed, 12.32s. |
| Release, `ENABLE_PJSIP=ON`, `BUILD_TESTS=ON` | Build completed; one MSVC warning found and fixed (C4804, see W110-F021) — otherwise clean. |
| Python tests / harnesses / E2E | None exist in this repo (no `.py` test files, no separate E2E harness beyond the CTest suite and the non-registered `live_*_probe` executables — see W110-F005). |

Full baseline logs: `build/nmake_build_full.log`, `build/ctest_baseline.log`
(pre-fix), `build-release/nmake_build.log`.

## Architecture

See [W110-architecture-map.md](W110-architecture-map.md) for the full
module inventory, ownership model, threading analysis, and 22 flow traces
(startup through shutdown).

**Modules analyzed:** app, core, emergency, etsi, gui, media, msrp, rtt,
security, sip (all 10 top-level `src/` directories).

**Lifecycle / ownership:** Reviewed SipManager, SipCall, SipAccount,
MsrpSession, MsrpTransport, MsrpRelayClient, MsrpCallPreparationController,
file-transfer classes, RttSession, MainWindow/panel lifecycle, and
application shutdown. One High finding (W110-F001, MSRP relay allocation
refresh orphaning) and one Low finding (W110-F017, timer-connection
accumulation) — both in the same MSRP relay lifecycle area. Everywhere
else (SipCall/pj::Call teardown, MsrpSession transport reassignment,
RttSession, AudioMediaManager, SipManager's active-call cleanup) was
verified sound: consistent `QPointer` guards, consistent disconnect-before-
destroy ordering, no use-after-free found.

**Threading / UI thread:** No `QThread`/`moveToThread`/`std::thread`
anywhere in `src/` — the app is single-threaded on the Qt event loop except
for pjsua2's own internal worker thread(s). Every PJSIP callback reviewed
marshals to the Qt thread via `QPointer` + `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)` before touching any Qt-owned state — verified, not
assumed, across `SipAccount.cpp` and `SipCall.cpp`. No direct UI mutation
from a PJSIP/socket/worker callback was found.

**SIP / dialog state:** Reviewed REGISTER, INVITE (in/out), provisional
responses, CANCEL/BYE/ACK, re-INVITE, hold/resume, transport selection.
One High finding: `Held → Failed` is not a legal transition, so a dialog
that dies while on hold got permanently stuck (W110-F002, **fixed**).
Double-hangup, post-terminated `getInfo()`, and re-INVITE ping-pong were
all checked and found already correctly guarded.

**SDP / media negotiation:** Reviewed m=audio/video/text/message
construction and renegotiation. One Medium finding: the initial-INVITE
answer path accepts RTT unconditionally regardless of the local profile's
RTT setting, inconsistent with the deliberate consent flow Task W109A built
for re-INVITE-time offers (W110-F006). MSRP/RTT media-index handling,
hold/resume state preservation, and the specific pjsip warnings named in
the task brief (`PJSIP_ETPNOTSUITABLE`, "ports connection already exists")
were checked and found to be already-documented, non-silent, non-buggy.

**Audio / video:** No lifecycle or device-handling bugs found in the areas
reviewed (device attach/detach is `QPointer`-guarded and idempotent); RTP
port isolation between same-host instances (Task W109A) was reconfirmed
correct including for MSRP's ephemeral-by-default port.

**RTT:** One Medium finding (W110-F006, above). The renegotiation
ping-pong bug and RTP port collision from Task W109A were reconfirmed
fixed and not regressed (full CTest pass including the extended
`test_rtt_session.cpp`).

**SIP MESSAGE / CPIM / IMDN / is-composing:** One Medium finding each on
IMDN delivery-state precedence (W110-F009) and CPIM line-ending RFC
conformance (W110-F010). Message-ID dedup, is-composing timer cleanup, and
Content-Length/byte-counting were all verified correct.

**Presence / XCAP:** No High/Medium findings. The W104 XCAP path-traversal
fix was reconfirmed applied uniformly; XML validation, retry/backoff, and
credential-logging were all verified sound.

**MSRP direct:** No High/Medium findings beyond the relay-specific ones
above. Connection-hijack protections from W105/W106 were reconfirmed
correctly applied.

**MSRP relay:** W110-F001 (High, allocation refresh orphaning) and
W110-F017 (Low, timer-connection leak). AUTH/digest handling, retry
bounding, and credential-logging were verified sound.

**File transfer:** One High finding (W110-F003, negotiated hash never
verified) and one Low finding (W110-F016, reserved filenames). Path
traversal was confirmed already prevented.

**Parsers / binary safety:** One Low finding (W110-F015, missing size cap
on two XML parsers, not independently exploitable). Deflate/gzip
decompression-bomb protection, MSRP frame/chunk bounds-checking, and
embedded-NUL/byte-counting handling were all verified sound — see
[W110-security-review.md](W110-security-review.md).

**Config / profile:** RTP port range and config-dir isolation (Task
W109A) reconfirmed correctly wired everywhere needed. Credential storage
verified to never touch plaintext ini. One Medium finding: dead
"Connection" Settings fields that are stored/exported but never applied
(W110-F011).

**UI:** Connect()-call audit across MainWindow/CallPanel found no true
duplicate signal/slot registrations (several signals are legitimately
connected more than once to distinct slots for distinct purposes). One
High finding: an async call-preparation failure path had no user-visible
error (W110-F004, **fixed**, reusing the existing `callFailed` signal and
UI wiring from the immediately-preceding commit `260a4e9`). A full
widget-by-widget manual click-through was not performed at this depth — see
coverage notes in [W110-findings.md](W110-findings.md).

**Diagnostics / logging / export:** Credential/Authorization/digest
redaction verified sound across raw SIP trace, MSRP diagnostics, and XCAP
URL logging. One Low doc-drift finding: stale `schemaVersion` references in
`docs/windows-trace-json-export.md` (W110-F019, **fixed**).

**Test suite audit:** See [W110-test-coverage-gaps.md](W110-test-coverage-gaps.md).
One Medium finding (a no-op test, W110-F012, **fixed**) and one High
structural finding (PJSIP real-backend paths `QSKIP`'d in CTest, W110-F005,
deferred to W115 — this is a test-infrastructure investment, not a
one-line fix).

**Static analysis:** No dedicated clang-tidy/cppcheck run was performed
(not available in this environment without a new toolchain install, which
would itself be a scope-creep risk this task's rules caution against);
instead, the Release build's own MSVC warning output was used as the
static-analysis signal, surfacing and fixing one real issue (W110-F021,
Qt6 `QHash::remove()` returns `bool` not a count).

**Security review:** See [W110-security-review.md](W110-security-review.md)
for the full threat model. No Critical-severity issue found.

**Documentation vs code:** See W110-F013 (tracked machine-specific
`CMakeUserPresets.json`) and W110-F014/F019 (stale doc references, both
fixed). No documented "complete"/"PASS" feature was found referencing a
function or file that no longer exists — spot-checked several
(`RtpPortRangeConfig`, `MsrpCallPreparationController`,
`EmergencyCallAdapter`, `PresenceResubscribePolicy`) and all matched the
current API.

## Findings summary

| Severity | Count | Fixed in W110 | Deferred |
|---|---|---|---|
| Critical | 0 | — | — |
| High | 5 | 2 (F002, F004) | 3 (F001 → W112, F003 → W113, F005 → W115) |
| Medium | 9 | 2 (F012, F014) | 7 (F006 → W111, F007/F008/F009/F010 → W114, F011 → W115, F013 → owner decision) |
| Low | 7 | 3 (F019, F021, F022) | 4 (F015/F016 → W114, F017 → W112, F018 → opportunistic) |
| **Total** | **21** | **7** | **14** |

(One additional item, W110-F020, is informational/not-a-bug and excluded
from the count — see [W110-findings.md](W110-findings.md).)

Full detail on every finding: [W110-findings.md](W110-findings.md).
Follow-up task definitions: [W110-remediation-roadmap.md](W110-remediation-roadmap.md).

## Fixes applied in this task

All six fixes are small, isolated, and covered by either a new/updated
regression test or (where a live/HAVE_PJSIP harness would be required and
none exists yet — see W110-F005) a documented manual code-path verification:

1. **W110-F002** — `SipCall::onCallState` now reroutes `Held → Failed`
   through `Disconnecting` (mirroring the existing `Held → Idle` reroute).
   Test added: `test_call_state_machine.cpp::heldToFailedRequiresDisconnectingReroute`.
2. **W110-F004** — `SipManager::dispatchMakeCall`'s relay-preparation
   `failed` lambda now emits `callFailed` directly, reusing existing UI
   wiring (`CallPanel::onCallFailed`, `MainWindow`'s status-bar/video-request
   cleanup handlers — all verified idempotent/safe to receive this signal
   when no dialog was ever active).
3. **W110-F012** — replaced a no-op `QVERIFY(true)` with a real assertion
   in `test_registration_state_machine.cpp`.
4. **W110-F014** — updated `docs/project-status.md`'s stale active-branch
   pointer (part of this task's own project-status update).
5. **W110-F019** — corrected stale `schemaVersion` references (2 → 3) in
   `docs/windows-trace-json-export.md`.
6. **W110-F021** — fixed a Qt6 `QHash::remove()` bool-vs-count comparison
   in `MsrpSessionStore.cpp`, surfaced by an MSVC warning during the
   Release build.
7. **W110-F022** — fixed a discarded `[[nodiscard]]` `QFile::open()` result
   in a test helper (`test_diagnostics_bundle.cpp`), also surfaced by the
   Release build.

No pjproject sources were modified. No architectural refactors were
performed — every other finding above Low severity that would require
more than an isolated, single-purpose change was deferred to the roadmap
tasks (W111–W115) per this task's own rules.

## Post-fix verification

| Configuration | Result |
|---|---|
| Debug, `ENABLE_PJSIP=ON`, `BUILD_TESTS=ON` — rebuild after fixes | 0 errors, 0 warnings (`build/nmake_build_final.log`). |
| CTest (Debug/PJSIP) after fixes | **77/77 passed**, 0 failed, 16.92s (`build/ctest_final_debug.log`). |
| Release, `ENABLE_PJSIP=ON`, `BUILD_TESTS=ON` — rebuild after fixes | 0 errors, 0 warnings (`build-release/nmake_build_final.log`) — confirms both W110-F021 (QHash::remove) and W110-F022 (nodiscard) are resolved. |
| CTest (Release/PJSIP) after fixes | **77/77 passed**, 0 failed, 14.70s (`build-release/ctest_release_final.log`). |

No regressions were observed in REGISTER, audio, video, RTT, SIP MESSAGE,
MSRP (direct or relay), CPIM, IMDN, is-composing, Presence, XCAP, file
transfer, hold/resume, diagnostics, or export — the full existing CTest
suite (which covers all of the above at the unit level, see
[W110-test-coverage-gaps.md](W110-test-coverage-gaps.md) for the caveat on
PJSIP-real-backend depth) passed unchanged plus the two updated/added
tests.

## Production readiness assessment

No Critical-severity defects were found. The two High-severity issues
fixed in this task (stuck-call-on-hold-failure, silent async
call-preparation failure) were both real, user-visible correctness bugs
now resolved with regression coverage. The three remaining High-severity
items (MSRP relay allocation lifecycle, file-transfer hash verification,
PJSIP-real-backend test coverage) are real gaps but architectural in
nature — each is scoped as its own follow-up task with acceptance criteria
in the roadmap rather than rushed into this review. The codebase overall
showed strong existing discipline (redaction, bounds-checking, path-
traversal, and decompression-bomb protections were consistently already in
place and correctly applied, not merely assumed) — this review's job was
largely to verify that discipline held under adversarial reading, and it
did, with the specific exceptions documented above.

## Git status / push

- Commits on this branch: fix commits for W110-F002/F004/F012/F021, plus
  documentation commits for W110-F014/F019 and this review's own
  docs/reviews/* additions and project-status.md update (see final commit
  list in `git log --oneline audit/w110-full-project-code-review`).
- `git status`: clean after the final commit (verified before push).
- Pushed: `git push -u origin audit/w110-full-project-code-review`.

## Confirmations

- Branch `audit/w110-full-project-code-review` has been pushed.
- No merge into `main` was performed (no `main` branch exists in this repo).
- No merge into `release` was performed (no `release` branch exists; only
  historical `release/vX.Y.Z` tags-as-branches, untouched).
- This review covered the entire project (all 10 `src/` modules, build
  system, test suite, documentation), not only the most recent task's
  changes.
- All findings are supported by evidence from code, tests, or reproducible
  behavior — no finding is speculative, and every "checked, found sound"
  claim in this document reflects an actual code read, not an assumption.
- No interoperability was declared without a real test backing it; where
  live interop retesting is warranted (W110-F010, CPIM CRLF) it is called
  out explicitly as required before that fix can be considered complete.
- Large/architectural fixes were not implemented in W110 — they are
  separated into dedicated follow-up tasks (W111–W115) with their own
  branches, risk assessment, and acceptance criteria.
- pjproject sources were not modified.
- No credential or other sensitive information was introduced into the
  repository by this task.
