# MSRP Live Interoperability (Task W102)

Top-level summary of Task W102 — validating and correcting the W100/W101
MSRP implementation using real SIP dialogs and MSRP connections. See the
per-phase documents this links to for detail, and
`docs/agent-results/W102-msrp-live-interoperability-result.md` for the full
42-point final report.

## What W102 implemented

- **Peer-initiated offer/answer** (`msrp-offer-answer.md`): a peer's own
  `m=message` offer is now answered with a real accepted section at the
  correct index (replacing PJSIP's own auto-generated rejected placeholder),
  instead of always being left rejected.
- **Setup role negotiation** (`msrp-offer-answer.md`, Phase 3): the answer's
  `a=setup` is always chosen as the structural complement of the remote's
  proposed role (active↔passive; `actpass`/unrecognized defaults to
  passive) — this rules out active/active and passive/passive collisions
  by construction, not by runtime detection. When the negotiated role
  requires this client to be the connecting (active) party,
  `SipCall::Impl::startMsrpActiveConnect` drives `MsrpSession::connectAsActive`
  using the peer's own advertised `a=path`.
- **Peer association** (`msrp-peer-association.md`, updated): every session
  now carries a real `role` (`MsrpRole::ActiveConnector`/`PassiveListener`,
  set by which connect/listen call was actually made — not inferred) in
  addition to the W101 `mediaIndex`/`sipHeaderCallId` mapping.
- **UI diagnostics**: the MSRP page's session table gained "Negotiation"
  and "Peer Association" columns, sourced from the same fields now in the
  JSON export.
- **Export schema v3** (`windows-trace-json-export.md`, updated): every
  `msrpSessions` entry gains `role`, `remoteSetup`, `negotiationState`, and
  a `peerAssociation` object.
- **Client/server comparator** (`client-server-trace-comparison.md`): a new,
  deterministic, unit-tested trace correlator.
- **TLS review** (`msrp-tls.md`): existing W100 TLS implementation audited
  against W102's explicit requirements; no code change needed on the
  active-connect side, server-side certificate provisioning gap confirmed
  still open.
- **Early dialogs/forking review** (`msrp-early-dialogs-and-forking.md`):
  pjsua2's single-`Call`-object model already structurally prevents the
  specific hazard the task calls out; no live forking test was possible in
  this environment.

## Simultaneous sessions to the same peer (Phase 5)

Each `SipCall` owns its own `MsrpSession`/listener, keyed by that call's own
identity (SIP Call-ID, media index) — two or three simultaneous calls to the
same peer each get their own `SipCall` instance and therefore their own,
independent MSRP session/listener/port. This was true structurally since
W101 and re-confirmed by code audit in W102, not newly built. **Not tested
live** (no multi-call test harness or live peer was available) — see below.
Two `m=message` sections *within the same call/SDP* are explicitly limited
to one live session (documented in msrp-offer-answer.md), a real
architectural constraint, not an untested gap.

## Transport policy / recovery (Phase 8)

Unchanged from W101: `SipManager::sendSipMessage` recomputes the transport
decision fresh on every send via `MessagingTransportPolicy` — there is no
cached/sticky fallback state, so "recovery back to MSRP once it becomes
available again" is a structural consequence of the design, not new W102
code. Re-audited in W102 and found still correct; no regression.

## Automated test results

68/68 CTest targets passing (67 from the W101 baseline + the new
`test_trace_comparator` target), zero regressions. New/changed assertions
this task added: `test_msrp_sip_media_injector` (+3: answer-at-index,
wrong-index rejection, out-of-range rejection),
`test_windows_trace_json_export` (+1: v3 fields), `test_trace_comparator`
(new target, 8 tests).

## Live interoperability — PASS / FAIL / BLOCKED / NOT RUN

No live SIP-Server-RTT, Blink, AG Projects, or Linphone instance was
reachable from this development environment in this session — exactly the
same situation as W101. Every live scenario in the task's Phase 12/13 list
is therefore:

| Scenario | Result |
|---|---|
| Client A → Client B via SIP-Server-RTT, full offer/answer/SEND/response/REPORT | `NOT RUN` — no server reachable |
| CPIM/IMDN/is-composing over live MSRP | `NOT RUN` — depends on the above |
| Fallback/recovery against a real peer | `NOT RUN` — depends on the above |
| Two simultaneous calls to a real peer | `NOT RUN` — depends on the above |
| Blink / AG Projects MSRP interop | `NOT RUN` — no such client/environment available |
| Linphone MSRP interop | `NOT RUN` — not claimed; Linphone's own MSRP support was not verified in this session either |
| Live forking scenario | `NOT RUN` — no forking-capable proxy/fixture available (see msrp-early-dialogs-and-forking.md) |
| Live TLS handshake | `NOT RUN` — no TLS-capable MSRP peer available (see msrp-tls.md) |

No interoperability claim beyond "the automated unit/protocol tests above
pass" and "the wire-bytes-level SDP injection tests pass" should be
inferred from this document. See the final report for the complete,
literal breakdown.
