# RTT Offer/Answer State Machine (Task W109A)

## Root cause of the "renegotiation loop"

`RttSession` (`src/rtt/RttSession.h/.cpp`) previously had a 5-state machine
(`Disabled`/`Offered`/`Negotiated`/`Active`/`Failed`) with **no state for
"a remote offer is pending"** and **no state for "declined"**. Meanwhile
`SipCall::requestRtt(bool)` was the *only* entry point used for both:

- the user requesting RTT locally, and
- the user clicking "Accept RTT" on an incoming request.

Both call sites shared one code path and one in-flight flag
(`rttRequestPendingLocal`), with no guard against calling it twice, and no
distinction in the public API between "I am requesting" and "I am accepting
your request". Combined with the RTP port collision (see
[rtp-port-range-configuration.md](rtp-port-range-configuration.md)), each
"Accept" attempt could fail at the transport layer while the UI/state model
had no way to represent that failure distinctly from "still pending" — the
observed contradictory `Incoming text request pending ... awaiting user
accept` log line *after* the app had already answered the offer with
`m=text 0`.

## What pjsua2 actually allows (why Model B, not Model A)

`onCallRxReinvite()` is a synchronous callback: the SDK builds the 200 OK
answer from `prm.opt` **as modified inside the callback**, before it
returns. There is no pjsua2 API to defer that answer to a later user action
— the offer is answered (accepted or declined) in the same callback
invocation that detects it. This confirms **Model B (reject-then-renegotiate)**
from the task's Faza 7 is the only model this pjsua2 build supports; Model A
(deferred answer) is not available without modifying pjproject, which this
task's rules forbid unless unavoidable.

So the existing flow (auto-decline `m=text 0` synchronously, then a
brand-new local re-INVITE if the user accepts) was already structurally
correct. The bug was that this renegotiation was never given its own name,
guard, or state — it was silently folded into "requestRtt(true)" as if it
were an ordinary local request.

## Fix: separate methods, explicit states

`SipCall` (`src/sip/SipCall.h/.cpp`) now exposes three distinct entry points:

| Method | Meaning | Preconditions |
|---|---|---|
| `requestRtt(bool)` | Local-initiated request only | Refuses while a remote offer is pending (`hasPendingIncomingRttRequest()`); refuses while a local RTT re-INVITE is already in flight |
| `acceptIncomingRttRequest()` | Accept a pending remote offer — sends a **new** local re-INVITE with `m=text` (the only way to turn "already declined" into "active") | Requires a pending remote offer; refuses if a local re-INVITE is already in flight |
| `rejectIncomingRttRequest()` | Reject a pending remote offer — **no new re-INVITE**; the decline was already sent by the auto-response. Only clears the pending flag and emits `rttRequestRejected()` | Requires a pending remote offer |

`RttSession`'s state machine was extended (`src/rtt/RttSession.h`):

```
Disabled            No call, RTT never offered, or call ended
RemoteOfferPending  Peer sent m=text via re-INVITE; auto-declined (m=text 0),
                    awaiting acceptIncomingRttRequest()/rejectIncomingRttRequest()
LocalOfferPending   We offered m=text (initial INVITE, requestRtt(), or
                    acceptIncomingRttRequest()) — awaiting the result
Negotiating         SDP negotiated RTT before; text stream currently inactive
                    (e.g. call on hold) — was "Negotiated" pre-W109A
Active              RTP text stream active
Rejected            A pending request (local or remote) was declined
Failed              Transport/port error, or negotiation timed out
```

Transitions:

```
Disabled --(remote re-INVITE m=text)--> RemoteOfferPending
RemoteOfferPending --(rejectIncomingRttRequest)--> Rejected
RemoteOfferPending --(acceptIncomingRttRequest)--> LocalOfferPending
LocalOfferPending --(text stream active)--> Active
LocalOfferPending --(remote declines / port error / timeout)--> Failed
Active --(text stream drops, call still up)--> Negotiating
Negotiating --(re-INVITE succeeds)--> Active
any --(call ends)--> Disabled
```

`Negotiating` was deliberately kept from the pre-W109A model rather than
collapsed away — call hold/resume relies on this exact "SDP negotiated but
stream inactive" distinction and this task must not regress it.

## Anti-ping-pong

- **Single-flight guard**: `m_impl->rttRequestPendingLocal` blocks a second
  `requestRtt()`/`acceptIncomingRttRequest()` while one is already in
  flight (duplicate button clicks/signals are logged and ignored, not
  queued).
- **Local vs remote disambiguation**: `requestRtt()` refuses outright while
  a remote offer is pending — it can no longer be silently reinterpreted
  as "accept".
- **Own-offer completion detection**: `onCallMediaState()` was extended
  (`SipCall.cpp`) to notice when *our own* pending local offer comes back
  with the text media inactive or removed entirely (`rttLocalOfferWasPending`
  / `textLineSeen` tracking) and emits `rttNegotiationFailed()` instead of
  leaving `rttRequestPendingLocal` stuck forever — this is what previously
  could leave the Accept/Request button permanently disabled after a
  declined or failed renegotiation.
- **Bounded wait**: `RttSession` starts a 12s timeout on entering
  `LocalOfferPending`; if no definitive outcome (Active/Rejected/Failed)
  arrives in that window, the state machine forces `Failed` itself rather
  than waiting indefinitely (Faza 9's "timeout controlat").
- There is no re-INVITE ping-pong at the protocol level in this pjsua2
  build: `onCallRxReinvite()` only fires for a *remote*-originated
  re-INVITE, never as a side effect of our own `reinvite()` call — so our
  own accept-driven re-INVITE cannot itself be misread as a new incoming
  request. The observed "loop" in the bug report was the RTP port collision
  (each Accept attempt failing at the transport layer) compounded by the
  contradictory pending-state bug above, not a SIP-level offer/answer loop.

## Diagnostics

`src/sip/RtpPortDiagnostics.h/.cpp` classifies a failed re-INVITE's
`pj::Error::reason` into `RtpPortInUse` / `RtpPortRangeExhausted` /
`RtpPortAllocationFailed` / `NotPortRelated`, logged alongside the
underlying error text so operators can immediately tell "the RTP range is
colliding with another process" apart from an unrelated SIP failure.
