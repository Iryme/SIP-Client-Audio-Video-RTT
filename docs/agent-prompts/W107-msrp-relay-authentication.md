# Agent Prompt — Task W107: MSRP Relay Authentication and RFC 4976 Integration

Condensed from the full task instructions (the complete, verbatim spec —
64-item final report checklist, 18 phases, full rules list — was provided
directly by the project owner in chat; this file summarizes it for the
repo's agent-prompt/agent-result convention rather than duplicating every
line).

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`. Parallel validation repo:
  `SIP-Server-RTT` (not available in this session).
- New branch: `feature/w107-msrp-relay-authentication`.
- Starting branch: `feature/w106-msrp-connection-resilience` (the actual
  name of the existing W106 branch — the prompt's literal
  `feature/w106-msrp-connection-hijack-dos-recovery` does not exist in this
  repo; the real W106 branch was used instead, confirmed via `git branch -a`).
- No merge into `main`/`release` without explicit request.

## Context

Tasks W100–W106 built and hardened direct MSRP end-to-end (parser/
serializer, TCP/TLS transport, live SDP integration, bidirectional offer/
answer, CPIM/IMDN/is-composing, file transfer, RFC 4975 §7.1 To-Path
hardening, connection-hijack DoS recovery) — 71/71 automated tests passing.
A separate sip2sip.info live-validation session (not itself part of a
numbered W-task) confirmed REGISTER/audio/video/RTT-degradation/XCAP
against real infrastructure, but explicitly left MSRP session establishment
with a real independent peer untested. A follow-up question — "MSRP works,
I tested it from the GUI, why do you say it's untested?" — led to
discovering sip2sip.info advertises a real RFC 4976 MSRP relay
(`msrprelay.sipthor.net:2855`, DNS-SRV-discoverable) that this client has
**zero AUTH-method support for**, meaning whatever "worked" in that manual
GUI test could not have been this relay path.

## Objective

Implement RFC 4976 MSRP relay authentication and integrate it into the
real SIP/MSRP flow, without affecting direct MSRP, with two selectable
modes (Direct / Relay-assisted) chosen by configuration/negotiation/
runtime state — never hardcoded values (relay host/port/credentials/realm/
nonce/session-id/paths must all come from profile/config/UI/environment/
negotiation, never literals in code).

## Permanent rules (paraphrased)

- No hardcoded relay/account/credential/session values anywhere.
- Don't modify vendored pjproject sources unless unavoidable and explicitly
  justified/isolated/tested.
- Don't break REGISTER/audio/video/RTT/SIP MESSAGE/direct MSRP/CPIM/IMDN/
  is-composing/presence/XCAP/file-transfer/diagnostics/export/SIP Ladder.
- Never block the UI thread; PJSIP/socket callbacks must not touch UI
  directly.
- Binary protocol data as `QByteArray`, never `QString`, for framing.
- Never log/export password/full credential hash/Authorization/
  Proxy-Authorization/full digest response/reusable nonces/private keys/
  full sensitive relay paths or URLs.

## 18 implementation phases (paraphrased)

1. Audit existing MSRP/SIP-digest code and any prior relay
   investigation/documentation before writing anything.
2. Relay configuration model: `Disabled` / `Automatic` / `Required` modes.
3. A separate `MsrpRelayClient`/`MsrpRelayAllocation`/`MsrpRelayAuthState`/
   `MsrpRelayDiagnostics` component — not folded into `MsrpSession` as a
   monolith. A defined state machine (Idle → ... → Allocated/Failed).
4. AUTH challenge/response flow with full error-path coverage (missing
   challenge fields, unsupported algorithm/qop, stale nonce, timeout,
   malformed response, relay unavailable, TLS failure).
5. Allocation/Use-Path extraction, validation, and dialog/session
   association — no path/port/session-id ever offered before real
   allocation.
6. SDP integration: relay allocation must complete *before* the SDP that
   offers it is built; never offer an unallocated relay path.
7. Dialog ↔ relay-allocation ↔ MSRP-session mapping supporting multiple
   simultaneous calls without cross-allocation confusion.
8. SEND/response/REPORT routed through the relay once allocated, with no
   duplicate transport use (relay + direct + SIP MESSAGE simultaneously)
   unless policy explicitly requires it.
9. Refresh/expiry lifecycle with a configurable refresh margin and cleanup
   on call end/logout/shutdown/network failure.
10. Recovery flow for control/transport failure, DNS change, TLS/auth
    failure, stale nonce, expired allocation, etc. — no automatic
    retransmission of already-confirmed messages.
11. Direct-MSRP compatibility: relay must never be forced on peers using
    direct MSRP, and must not change direct MSRP's active/passive role
    semantics.
12. TLS: certificate chain/hostname validation, custom CA, explicit
    self-signed test mode, no silent verification bypass.
13. Experimental, off-by-default relay diagnostics UI panel (mode,
    connection/auth/allocation state, expiry, refresh/reconnect counts,
    redacted paths — never raw credentials/nonces/tokens).
14. Additive JSON export extension (`msrpRelayEvents`/
    `msrpRelayAllocations`/`msrpRelayAuth`/etc.), schema version bumped
    only on genuine incompatibility, all sensitive fields redacted.
15. Client/server trace comparator extension for relay-specific
    correlation (deferred — no SIP-Server-RTT relay trace available to
    compare against in this session).
16. Full automated test coverage: AUTH, allocation, SDP, mapping,
    transport, TLS, lifecycle, security, plus a full W090–W106 regression
    run.
17. Live test against sip2sip.info, credentials via environment/secure
    store only, never committed — 20 listed minimal scenarios, using
    strict `PASS`/`FAIL`/`BLOCKED`/`NOT RUN`/`UNSUPPORTED` status labels,
    with `UNSUPPORTED` only when lack of support is confirmed and no PASS
    ever claimed without direct evidence of a successful AUTH/allocation.
18. Live test against SIP-Server-RTT if available; otherwise report
    `BLOCKED`/`NOT RUN`, never a fabricated fixture presented as live
    interop.

## Required documentation

New: `msrp-relay-authentication.md`, `msrp-relay-allocation.md`,
`msrp-relay-security.md`, `msrp-relay-testing.md`,
`msrp-relay-sip2sip-validation.md`. Updates: `project-status.md`,
`msrp-security.md`, `msrp-testing.md`, this prompt/result pair (the full
instruction also lists updates to several other MSRP docs — see the result
doc's "Limitations" for which were and were not touched given session
scope).

## Final report requirements (64 items) and mandatory confirmation block

The full instructions specify an exhaustive 64-item report checklist and a
mandatory closing confirmation block (branch pushed; no merge to
main/release; direct MSRP retested; no test credential committed; no relay
interoperability declared PASS without live evidence; pjproject sources
unmodified or explicitly justified). See
[agent-results/W107-msrp-relay-authentication-result.md](../agent-results/W107-msrp-relay-authentication-result.md)
for the actual result against every one of these.
