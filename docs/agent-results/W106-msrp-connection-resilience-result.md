# Agent Result — Task W106: MSRP Connection-Hijack DoS Recovery

## 1. Branch

`feature/w106-msrp-connection-resilience`, branched from
`feature/w105-msrp-advanced-hardening`. Not merged into `main`/`release`/any
other branch.

## 2. Version / schema

No `InteropTraceExporter::kSchemaVersion` change.

## 3. Vulnerability found and fixed

Task W105 stopped a rogue local connection from injecting MSRP messages
(RFC 4975 §7.1 To-Path validation), but left a more severe consequence of
the same root cause open: `MsrpTcpTransport`/`MsrpTlsTransport` stop
listening for good the moment they accept their first inbound TCP
connection (`onNewConnection` calls `m_server->close()`). A rogue local
process that simply connects first — without ever sending a valid MSRP
request — would previously deny service to the real, SDP-negotiated peer
permanently: the listener was already closed, so the real peer's later
connection attempt would just be refused by the OS.

## 4. What was implemented

- `src/msrp/MsrpTransport.h` — new pure-virtual `relisten()`: drop the
  current socket without emitting `disconnected()`/`errorOccurred()` for
  that drop, then resume listening on the same bind address/port for
  whatever time remains of the original `acceptTimeoutMs`.
- `src/msrp/MsrpTcpTransport.h/.cpp` and `src/msrp/MsrpTlsTransport.h/.cpp`
  — both implement `relisten()` identically: remember `bindAddress`,
  `serverPort()` (resolved, not the original possibly-0 request), and the
  original accept timeout via a `QElapsedTimer` started in
  `listenPassive()`. `relisten()` aborts+deletes the old socket (signals
  disconnected first, so the abort itself is silent), then reopens the
  `QTcpServer` on the same address/port and restarts the accept timer with
  the remaining budget. If the budget has already elapsed, it emits the
  same `"accept timeout: no peer connected"` error as an ordinary failed
  passive listen instead of relistening.
- `src/msrp/MsrpSession.h/.cpp` — new `m_awaitingAuthentication` member:
  set `true` in `listenAsPassive()`, cleared the moment any inbound request
  passes `toPathTargetsThisSession()` in `handleFrame()`.
  `rejectUnauthorizedRequest()` now calls `m_transport->relisten()` (and
  resets state to `Connecting`) whenever it rejects a request while
  `m_awaitingAuthentication` is still `true` — i.e. only before this
  session has ever accepted one genuine peer. Once authenticated, later
  drops go through the normal `onTransportDisconnected()`/`onTransportError()`
  close/failure path unchanged.

## 5. Files changed

### Added
- `docs/agent-prompts/W106-msrp-connection-resilience.md`
- `docs/agent-results/W106-msrp-connection-resilience-result.md` (this file)

### Modified
- `src/msrp/MsrpTransport.h` — new `relisten()` pure virtual.
- `src/msrp/MsrpTcpTransport.h/.cpp` — `relisten()` implementation,
  tracking fields.
- `src/msrp/MsrpTlsTransport.h/.cpp` — `relisten()` implementation,
  tracking fields (mirrors the TCP transport).
- `src/msrp/MsrpSession.h/.cpp` — `m_awaitingAuthentication`, the
  `relisten()` call from `rejectUnauthorizedRequest()`.
- `tests/test_msrp_session_harness.cpp` —
  `relistensAfterRejectedConnectionAllowsRealPeer`.
- `docs/msrp-security.md` — new "Connection-hijack denial-of-service
  recovery (Task W106)" section; updated the "not hardened" connection-flood
  bullet to reflect what W106 does and does not cover.
- `docs/project-status.md` — task table + module status row updates.

## 6. Automated tests

Full build (`build_w101.bat`) completed with zero errors. Full regression
(`ctest_w101.bat`): **71/71 tests passed, 0 failed** (same target count as
W105 — the new case was added to the existing `test_msrp_session_harness`
target), zero regressions.

New test: `relistensAfterRejectedConnectionAllowsRealPeer` — a rogue
connection connects first with a fabricated session-id and is rejected
(reusing the same setup as W105's `rejectsSendWithMismatchedToPathSessionId`),
then asserts the server has resumed listening on the exact same port, then
has a second, correctly-addressed session connect and successfully
exchange a real message end-to-end (`payloadReceived` fires with the
expected body).

All 6 pre-existing `test_msrp_session_harness` cases (including W105's
rejection test) continue to pass unchanged.

## 7. Manual/live interoperability tests

`NOT RUN`. No SIP-Server-RTT instance or third-party MSRP client is
reachable from this environment (same limitation documented in every MSRP
task since W100). This task's fix and its regression test are both
verifiable entirely with local loopback TCP, which was exercised (see §6).

## 8. PASS / FAIL / BLOCKED / NOT RUN summary

| Item | Result |
|---|---|
| Rejected/rogue connection triggers relisten on the same port | PASS (`relistensAfterRejectedConnectionAllowsRealPeer`) |
| Real peer connects and exchanges a message after relisten | PASS (same test) |
| Already-authenticated session is unaffected by relisten logic on later disconnect | PASS (all other pre-existing cases, none of which trigger the rejection path, still pass unchanged) |
| Rejection/message-injection behavior from W105 unaffected | PASS (`rejectsSendWithMismatchedToPathSessionId` still passes unchanged) |
| Live interoperability (real peer/relay) | NOT RUN — no environment available |
| Existing MSRP/SIP/audio/video/RTT/messaging/presence/XCAP/diagnostics functionality | PASS (unchanged, zero regressions, 71/71) |

## 9. Client problems vs. server problems

No server was reached (none available). Not a defect report.

## 10. Build issues found and fixed during this task

None.

## 11. Regressions

None. 71/71 tests passing, zero regressions.

## 12. Limitations

- Bounded, not unlimited: relistening only happens while
  `m_awaitingAuthentication` is still true and only for the remainder of
  the original accept window — a rogue peer cannot extend that window by
  repeatedly reconnecting-and-failing, but it can still consume CPU with
  reconnect/reject cycles within it. A dedicated rate limiter for that
  remains an open, documented gap (see `docs/msrp-security.md`).
- Best-effort 403 delivery to the rejected peer: `relisten()` aborts the
  socket immediately after `rejectUnauthorizedRequest()` queues the 403
  response bytes; delivery to a genuinely hostile/stale peer is not
  guaranteed, and was never guaranteed by W105 either. This does not
  affect the legitimate peer's own eventual connection or its responses.
- Server-side (passive/listening) TLS certificate provisioning remains the
  same documented gap from W100/W102/W105 (`docs/msrp-tls.md`) — untouched
  by this task.
- No live interoperability test executed (see §7).

## 13. What moves to a future task

Server-side TLS certificate provisioning and a dedicated connection-flood
rate limiter (bounding *how many* reconnect-and-reject cycles are allowed
within one accept window, not just when they stop) remain open, documented
gaps — not implicitly part of this task and not claimed as fixed here.

## 14. Commits

Thematic commits on this branch (see `git log feature/w106-msrp-connection-resilience`):

1. `fix(msrp): relisten after a rejected inbound connection instead of
   permanently denying the real peer` — `MsrpTransport::relisten()`,
   `MsrpTcpTransport`/`MsrpTlsTransport` implementations,
   `MsrpSession::m_awaitingAuthentication` wiring.
2. `test(msrp): cover relisten recovery after a rejected connection` —
   `test_msrp_session_harness` addition.
3. `docs(msrp): document w106 connection-hijack DoS recovery` —
   `docs/msrp-security.md`, `docs/project-status.md`,
   agent-prompts/agent-results.

(Exact commit hashes are visible in `git log` on this branch.)

## 15. git status (post-work)

Clean except the same pre-existing untracked `.bat` helper scripts present
since before this task began (not new artifacts of this task).

## 16. Push status

Pushed to `origin/feature/w106-msrp-connection-resilience`.

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
