# Agent Prompt — Task W109A: Fix RTT Renegotiation Loop and Same-Host RTP Port Collisions

Condensed from the full task instructions (the complete, verbatim spec —
16 phases, full permanent-rules list, mandatory Romanian confirmation
block, 45-item final report checklist — was provided directly by the
project owner in chat; this file summarizes it for the repo's
agent-prompt/agent-result convention rather than duplicating every line).

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `fix/w109a-rtt-renegotiation-and-rtp-port-collision`.
- Starting branch: `feature/w108-relay-live-call-integration` (no W109
  branch/work existed yet at start — confirmed via `git branch -a` and
  `git log --all --oneline | grep -i rtt`).
- No merge into `main`/`release` without explicit request. (No `main`
  branch exists in this repo at all — see `docs/project-status.md`'s
  Branch Lineage Note.)

## Context

Testing a call between two clients on the same Windows machine: audio
works, but requesting RTT enters a renegotiation loop and the RTT stream
never activates. Log evidence showed `WSAEADDRINUSE` on RTP ports
4002/4004/4006/4008/4010, and a contradictory "still pending" state after
the app had already declined the offer with `m=text 0`.

## Objective

Fix two distinct problems: (1) RTP port collision when two instances run
on the same host, (2) the incorrect RTT accept workflow producing
re-INVITE ping-pong / repeated requests. Permanent rules: no hardcoded
ports/IPs/URIs/ranges/profile names — everything from config/CLI/env; do
not modify pjproject unless unavoidable; do not block the UI thread; do
not update UI directly from PJSIP callbacks; do not regress
REGISTER/audio/video/initial-RTT-offer/SIP MESSAGE/MSRP/CPIM/IMDN/
is-composing/Presence/XCAP/file-transfer/hold-resume/diagnostics/export.

## Phases (as given)

1. Baseline + reproduction (git status/branch/log, full build, two-instance
   repro, collect raw SDP/state transitions/re-INVITE count).
2. Audit RTP port allocation (pjsua2 config, per-process defaults,
   configurability, logging).
3. Add configurable RTP range (config fields, validation, CLI/env
   overrides, pre-libInit/libStart application, effective-range logging).
4. Detect port collision without silent/unbounded retry; distinct
   diagnostics (`RtpPortAllocationFailed`/`RtpPortRangeExhausted`/
   `RtpPortInUse`); keep audio alive on RTT failure.
5. Audit the RTT offer/answer flow (`onCallRxReinvite`, accept button,
   `requestRtt()`, state machine, local-vs-remote offer identification).
6. Separate `requestRtt()` (local) from `acceptIncomingRttRequest()`/
   `rejectIncomingRttRequest()` (remote-offer response).
7. Determine which model pjsua2 actually supports (deferred-answer vs
   reject-then-renegotiate) from the real API, not assumption.
8. Introduce/correct an RTT negotiation state machine correlated with
   Call-ID/CSeq/direction/generation, distinguishing this call's own vs
   stale callbacks.
9. Anti-ping-pong protections: single pending local re-INVITE, ignore
   duplicate clicks, no auto-retry after reject, bounded timeout.
10. SDP validation for accepted/rejected RTT and untouched audio/video/MSRP.
11. Confirm RTT (`m=text`) and MSRP (`m=message`) are handled and diagnosed
    independently.
12. Same-host support: distinct config/profile dir, RTP range, and
    (already OS-assigned) SIP port per instance; document with an example.
13. UI states for each RTT phase; disable Accept after click; keep audio
    alive on RTT failure; mutually exclusive local-request vs
    remote-accept UI actions.
14. Investigate secondary warnings (`PJSIP_ETPNOTSUITABLE`, "ports
    connection already exists", post-terminated `getInfo()`) without
    expanding scope beyond RTT/port-collision.
15. Automated tests across all of the above; run full existing suite.
16. Mandatory manual same-host two-client test, or NOT RUN/BLOCKED with a
    concrete reason.

## Final report / git requirements

45-item structured report (branch, root causes old/new config, state
machine, SDP before/after, regression status, PASS/FAIL/BLOCKED/NOT RUN
per item, commits, push status). Push
`fix/w109a-rtt-renegotiation-and-rtp-port-collision` to origin; do not
merge; explicitly confirm no RTP ports were hardcoded, two instances can
use distinct ranges, Accept/Request have separate flows (or the
architectural difference is documented), no uncontrolled re-INVITE loop,
audio/video/MSRP retested, and whether pjproject sources were touched
(they were not).

See [W109A-rtt-renegotiation-and-rtp-port-collision-result.md](../agent-results/W109A-rtt-renegotiation-and-rtp-port-collision-result.md)
for the actual outcome.
