# Agent Prompt — Task W105: MSRP Advanced Hardening

## Context

Task W104 (MSRP File Transfer) shipped with 71/71 tests passing and flagged
`Task-W105 — Advanced MSRP Hardening` as the next roadmap item, without
prescribing its exact scope (see
`docs/agent-results/W104-msrp-file-transfer-result.md` §13).

## Scope decision

While auditing the codebase for concrete, well-defined hardening candidates
(not speculative ones), `MsrpTcpTransport::listenPassive()` was found to
accept the first inbound TCP connection unconditionally, and
`MsrpSession::handleFrame()` processed any inbound SEND/REPORT request from
that connection without validating who it came from. RFC 4975 §7.1 requires
a receiver to check that a request's `To-Path` identifies its own endpoint
before acting on it — this was not implemented. Any local process able to
connect to the ephemeral listening port before the real, SDP-negotiated
peer would previously be silently trusted and could inject MSRP messages
into an active session.

This is the scope of Task W105: implement and test that one check. Other
previously-documented MSRP gaps (server-side TLS certificate provisioning,
resumable file transfer, live-call wiring, connection-flood rate limiting)
remain explicitly out of scope — each is its own future task if requested,
not implicit W105 work.

## Requirements

- Add `MsrpSession::toPathTargetsThisSession()` — compares the inbound
  request's `To-Path` last URI's session-id against the session's own
  negotiated local session-id.
- Reject (`403 Forbidden`) and log (`MsrpDiagnosticsEvent`, `Kind::Error`)
  any inbound SEND/REPORT request that fails this check, before it reaches
  the chunk assembler or any `payloadReceived`/`fileTransferReceived`
  signal.
- Must not affect a legitimate peer: the To-Path a real peer sends is
  always built from this client's own advertised `a=path`, so normal
  SEND/REPORT/response traffic must be completely unaffected.
- Automated test proving the rejection (a session that connects but never
  learned the real negotiated session-id, simulating a rogue local process
  winning the TCP accept race) — no live peer/relay required.
- No change to `InteropTraceExporter::kSchemaVersion` (this is a security
  fix, not a new diagnostics field).
- Update `docs/msrp-security.md`, `docs/project-status.md`, and this
  prompt's paired result doc.
- Full build + full CTest regression (zero regressions), thematic commits,
  push to `feature/w105-msrp-advanced-hardening`, no merge.
