# Agent Prompt — Task W106: MSRP Connection-Hijack DoS Recovery

## Context

Task W105 (RFC 4975 §7.1 To-Path validation — see
`docs/agent-results/W105-msrp-advanced-hardening-result.md`) fixed a
message-injection gap: a rogue local process winning the TCP accept race
before the real, SDP-negotiated peer could no longer get any inbound
SEND/REPORT accepted. That task's own §13 ("What moves to a future task")
and §12 ("Limitations") left "connection-flood rate limiting" as an
explicitly open, undone item.

## Scope decision

Auditing the fix further: W105 blocks message injection but does not close
the underlying denial-of-service. `MsrpTcpTransport`/`MsrpTlsTransport`
stop listening for good the moment they accept their first TCP connection
(`onNewConnection` calls `m_server->close()`). So a rogue process that
merely connects first — without ever sending anything — permanently
prevents the real peer from connecting at all, even though W105 already
guarantees it could never have injected a message. This is a more severe
consequence of the same root cause W105 addressed, and is concrete,
well-scoped, and testable without a live peer/relay.

This is the scope of Task W106: when an inbound request is rejected under
the To-Path check *and* this session has never yet accepted a single
authenticated request, resume listening (on the same bind address/port,
for whatever time remains of the original accept window) instead of
leaving the session permanently unable to accept the real peer. Other
still-open gaps (server-side TLS certificate provisioning, a dedicated
connection-flood rate limiter for repeated rapid reconnect attempts) remain
out of scope — future tasks if requested, not implicit W106 work.

## Requirements

- Add `MsrpTransport::relisten()` (pure virtual), implemented in both
  `MsrpTcpTransport` and `MsrpTlsTransport`: drop the current socket
  without emitting `disconnected()`/`errorOccurred()` for that drop, then
  resume listening on the same bind address/port for the remaining accept
  budget.
- `MsrpSession` calls `relisten()` from `rejectUnauthorizedRequest()`, but
  only while no request has yet passed the To-Path check for this session
  (`m_awaitingAuthentication`) — an already-authenticated session's later
  disconnects must still go through the normal close/error path, not be
  relistened.
- Must not affect a legitimate peer's normal connect/authenticate/exchange
  flow.
- Automated test proving: a rogue connection is rejected, the server
  resumes listening on the same port, and a real peer connecting
  afterwards still establishes and exchanges a message successfully — no
  live peer/relay required.
- No change to `InteropTraceExporter::kSchemaVersion`.
- Update `docs/msrp-security.md`, `docs/project-status.md`, and this
  prompt's paired result doc.
- Full build + full CTest regression (zero regressions), thematic commits,
  push to `feature/w106-msrp-connection-resilience`, no merge.
