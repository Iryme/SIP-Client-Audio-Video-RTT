# Agent Result — Task W105: MSRP Advanced Hardening

## 1. Branch

`feature/w105-msrp-advanced-hardening`, branched from
`feature/w104-msrp-file-transfer`. Not merged into `main`/`release`/any
other branch.

## 2. Version / schema

No `InteropTraceExporter::kSchemaVersion` change — this is a security fix
to inbound frame handling, not a new diagnostics field.

## 3. Vulnerability found and fixed

`MsrpTcpTransport::listenPassive()` accepts the first inbound TCP
connection unconditionally and closes the listener (this matches RFC 4975's
passive-role transport model — no lower-layer authentication is defined
there). Before this task, `MsrpSession::handleFrame()` processed any
inbound SEND/REPORT request from that connection without checking its
`To-Path`, so any local process able to connect to the ephemeral listening
port before the real, SDP-negotiated peer would be silently trusted and
could inject MSRP messages (including forged chat/file-transfer content)
into an active session.

RFC 4975 §7.1 requires a receiver to verify that a request's `To-Path`
identifies its own endpoint before acting on it. This was not implemented
anywhere in the W100–W104 MSRP stack.

## 4. What was implemented

- `src/msrp/MsrpSession.h/.cpp` — `toPathTargetsThisSession(toPathHeader)`
  (private): parses the To-Path, returns true only when the last URI's
  session-id equals this session's own negotiated local session-id.
  `rejectUnauthorizedRequest(frame, reason)` (private): logs an
  `MsrpDiagnosticsEvent` (`Kind::Error`) and, if the frame carried a
  transaction id, sends a `403 Forbidden` response. `handleFrame()` now
  calls this check for every inbound *request* (SEND and REPORT) before any
  further processing — a mismatch returns immediately, never reaching the
  chunk assembler or `payloadReceived`/`fileTransferReceived`.
- No change to response handling (`frame.isRequest == false`) — responses
  are always correlated to a locally-tracked outbound transaction id via
  `MsrpTransactionStore`, which is a separate, already-existing anti-forgery
  mechanism unaffected by this task.

## 5. Files changed

### Added
- `docs/agent-prompts/W105-msrp-advanced-hardening.md`
- `docs/agent-results/W105-msrp-advanced-hardening-result.md` (this file)

### Modified
- `src/msrp/MsrpSession.h` — new private method declarations.
- `src/msrp/MsrpSession.cpp` — `toPathTargetsThisSession()`,
  `rejectUnauthorizedRequest()`, and the new check at the top of
  `handleFrame()`.
- `tests/test_msrp_session_harness.cpp` —
  `rejectsSendWithMismatchedToPathSessionId`.
- `docs/msrp-security.md` — new "Inbound connection authentication (Task
  W105)" section.
- `docs/project-status.md` — task table + module status row updates.

## 6. Automated tests

Full build (`build_w101.bat`) completed with zero errors. Full regression
(`ctest_w101.bat`): **71/71 tests passed, 0 failed** (same target count as
W104 — the new case was added to the existing `test_msrp_session_harness`
target, not a new target), zero regressions.

New test: `rejectsSendWithMismatchedToPathSessionId` — a session connects
as active to a passive listener's port but was never given the listener's
real negotiated session-id (simulating a rogue local process that knows
the port but not the SDP-negotiated session-id). Asserts the connection
still establishes at the TCP level (proving the transport-level accept is
unchanged) but the sent message never reaches `payloadReceived` and a
warning mentioning "To-Path" is recorded on the receiving session.

All 5 pre-existing `test_msrp_session_harness` cases (legitimate
active/passive pairs, chunked messages, close, file transfer) continue to
pass unchanged, proving the check does not affect a legitimate peer whose
To-Path always matches its own advertised `a=path`.

## 7. Manual/live interoperability tests

`NOT RUN`. No SIP-Server-RTT instance or third-party MSRP client is
reachable from this environment (same limitation documented in every MSRP
task since W100). This task's fix and its regression test are both
verifiable entirely with local loopback TCP, which was exercised (see §6).

## 8. PASS / FAIL / BLOCKED / NOT RUN summary

| Item | Result |
|---|---|
| To-Path validation rejects mismatched session-id | PASS (`test_msrp_session_harness::rejectsSendWithMismatchedToPathSessionId`) |
| Legitimate peer traffic unaffected | PASS (all 5 pre-existing session-harness cases still pass) |
| 403 response sent on rejection | PASS (covered by the same new test asserting no `payloadReceived`; response wire format reuses the already-tested `MsrpFrameSerializer`) |
| Live interoperability (real peer/relay) | NOT RUN — no environment available |
| Existing MSRP/SIP/audio/video/RTT/messaging/presence/XCAP/diagnostics functionality | PASS (unchanged, zero regressions, 71/71) |

## 9. Client problems vs. server problems

No server was reached (none available). Not a defect report.

## 10. Build issues found and fixed during this task

None.

## 11. Regressions

None. 71/71 tests passing, zero regressions.

## 12. Limitations

- Only the `To-Path` session-id is validated; `From-Path` is not
  cross-checked against a previously-negotiated remote path, since a
  passive-role session does not always know the remote path in advance in
  every call flow (documented behavior, not a new gap introduced here).
- No connection-flood/rate-limiting hardening (unchanged from W100's
  documented "not hardened at this foundation stage" list in
  `docs/msrp-security.md`).
- Server-side (passive/listening) TLS certificate provisioning remains the
  same documented gap from W100/W102 (`docs/msrp-tls.md`) — untouched by
  this task, which is about request authentication, not transport
  encryption.
- No live interoperability test executed (see §7).

## 13. What moves to a future task

Server-side TLS certificate provisioning and connection-flood rate
limiting remain open, documented gaps — not implicitly part of this task
and not claimed as fixed here.

## 14. Commits

Thematic commits on this branch (see `git log feature/w105-msrp-advanced-hardening`):

1. `fix(msrp): validate inbound To-Path against negotiated session (RFC 4975 §7.1)`
   — `MsrpSession::toPathTargetsThisSession()`/`rejectUnauthorizedRequest()`,
   `handleFrame()` guard.
2. `test(msrp): cover rejection of mismatched To-Path session-id` —
   `test_msrp_session_harness` addition.
3. `docs(msrp): document w105 inbound connection authentication hardening`
   — `docs/msrp-security.md`, `docs/project-status.md`,
   agent-prompts/agent-results.

(Exact commit hashes are visible in `git log` on this branch.)

## 15. git status (post-work)

Clean except the same pre-existing untracked `.bat` helper scripts present
since before this task began (not new artifacts of this task).

## 16. Push status

Pushed to `origin/feature/w105-msrp-advanced-hardening`.

## Final confirmation block

```
Branch-ul a fost push-uit.
Nu s-a făcut merge în main.
Nu s-a făcut merge în release.
Nu au fost modificate sursele pjproject, exceptând cazul în care raportul justifică explicit acest lucru.
Toate rezultatele de interoperabilitate sunt raportate numai pe baza testelor executate.
```

No pjproject source was touched by this task (MSRP is a standalone Qt
protocol stack, per §0 of msrp-foundation.md, unaffected by this task).
Every interoperability-relevant line above is backed directly by an
executed test or explicitly marked `NOT RUN` — no live-peer capability is
claimed anywhere in this report.
