# Agent Prompt — Task W108: Async MSRP Relay Allocation and Live Call SDP Integration

Condensed from the full task instructions (the complete, verbatim spec —
70-item final report checklist, 17 phases, full rules list, mandatory
Romanian confirmation block — was provided directly by the project owner
in chat; this file summarizes it for the repo's agent-prompt/agent-result
convention rather than duplicating every line).

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `feature/w108-relay-live-call-integration`.
- Starting branch: `feature/w107-msrp-relay-authentication`.
- No merge into `main`/`release` without explicit request.

## Context

W107 built a complete, standalone RFC 4976 relay AUTH client
(`MsrpDigestAuth`, `MsrpRelayClient`, allocation/refresh/recovery,
diagnostics/export, experimental UI panel) — 74/74 CTest, zero regressions
— but deliberately did not wire it into any real call's SDP flow, reasoning
that `SipCall`'s `onCallSdpCreated` pjsip callback is synchronous while
relay allocation is inherently asynchronous, and that this needed live GUI
testing capability the W107 session did not have. Live-tested against
sip2sip.info: TLS + initial AUTH/401 challenge confirmed working; the
authenticated AUTH retry got no response (FAIL, reported honestly).

## Objective

Integrate the W107 relay client into the real call lifecycle: resolve
direct-vs-relay asynchronously *before* the SIP call is created, so
`onCallSdpCreated` only ever reads an already-resolved decision (never
performs I/O or blocks), inject the resulting path into real SDP wire
bytes, bind the relay's connection to the call's `MsrpSession`, and handle
cancellation/races/fallback without regressing audio/video/RTT/direct MSRP.

## Mandatory architecture (paraphrased)

`onCallSdpCreated` must stay synchronous/deterministic/I/O-free. The
correct order is: resolve policy → (if relay) start async allocation → wait
async for the result → build a prepared/immutable SDP context → create the
SIP call → `onCallSdpCreated` injects the already-prepared offer → INVITE
goes out. Never send an INVITE before allocation when relay is required;
never invent a placeholder relay path.

## Permanent rules (paraphrased)

Same no-hardcoding/no-blocking/threading/compatibility/security rules as
W107 (see [W107's prompt](W107-msrp-relay-authentication.md)), applied to
the new call-preparation code specifically: no busy-wait/nested event
loop/`waitForConnected`/`waitForReadyRead`/sleep/polling; PJSIP/socket
callbacks never touch UI directly; never break REGISTER/audio/video/RTT/
SIP MESSAGE/direct MSRP/CPIM/IMDN/is-composing/Presence/XCAP/file
transfer/diagnostics/export/SIP Ladder; never log/export
password/digest/Authorization/nonce/token/sensitive path/credential.

## 17 implementation phases (paraphrased)

1. Baseline build/ctest + audit `SipManager::makeCall`, `SipCall`
   lifecycle, `onCallSdpCreated`, inbound/answer flow, threading.
2. A call-preparation state machine (`Idle → ResolvingPolicy →
   AllocatingRelay/PreparingDirectMsrp → Ready/Failed/Cancelled/Expired`).
3. An immutable, already-resolved "prepared MSRP offer" context handed to
   `SipCall` before the call is created; `onCallSdpCreated` only reads it.
4. Outbound call integration: Disabled/Automatic/Required policies, with
   Automatic falling back to direct MSRP on relay failure and Required
   never sending an INVITE without a valid allocation.
5. UX states during preparation (not implemented this session — no UI
   surface for relay config exists yet either, see the result doc).
6. Cancellation and race handling with a generation-token mechanism.
7. Inbound call integration — attempt, and report the real status honestly
   if not completed (see result doc: BLOCKED).
8. SDP injection verified at the wire-bytes level where feasible.
9. Relay transport adoption into the call's `MsrpSession`, with explicit,
   documented ownership.
10. Live SEND through an established relay-bound call.
11. Fallback policy without duplicate transport use.
12. Allocation refresh during an active call (documented as not fully
    wired — see result doc).
13. Diagnostics/export extensions (additive).
14. Client/server trace comparator extension (deferred — no
    SIP-Server-RTT trace available).
15. Investigate the W107 live authenticated-AUTH silent-drop failure.
16. Automated test coverage + full W090–W107 regression run.
17. Live validation against sip2sip.info / SIP-Server-RTT / real
    audio-video-RTT, using strict PASS/FAIL/BLOCKED/NOT RUN/UNSUPPORTED
    labels, never declaring live PASS without allocation + real SDP wire +
    real dialog + real SEND.

## Required documentation

New: `msrp-relay-call-preparation.md`, `msrp-relay-live-call-integration.md`,
`msrp-relay-async-sdp-architecture.md`, `msrp-relay-call-lifecycle.md`,
`msrp-relay-live-validation.md`. Updates: `project-status.md` (splitting
the single W107 "MSRP Relay Authentication" status row into AUTH
client/allocation/SDP integration/transport binding/live interop),
`msrp-testing.md`, `msrp-security.md`, this prompt/result pair.

## Final report requirements (70 items) and mandatory confirmation block

The full instructions specify an exhaustive 70-item report checklist and a
mandatory closing confirmation block (branch pushed; no merge to
main/release; relay allocation not in the synchronous SDP callback;
callback performs no I/O and does not block; direct MSRP preserved and
retested; audio/video/RTT retested; no credential committed; no relay
live result declared PASS without allocation + SDP wire + real dialog;
pjproject sources unmodified or explicitly justified). See
[agent-results/W108-relay-live-call-integration-result.md](../agent-results/W108-relay-live-call-integration-result.md)
for the actual result against every one of these.
