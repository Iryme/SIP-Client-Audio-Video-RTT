# MSRP and Early Dialogs / Forking (Task W102 Phase 6)

## What was audited

`SipCall`'s architecture (from W095/W101) creates exactly one `SipCall` /
`PjCall` per pjsua2 `Call` object, and `onCallSdpCreated`/`onCallState` are
both scoped to that one `pjsua2::Call` instance. The MSRP session created in
`Impl::createMsrpSessionObject()` is likewise a member of that one
`SipCall::Impl` — there is no global/static MSRP session registry keyed by
anything forkable (IP, port, or otherwise) that a second early-dialog branch
could accidentally bind into. This was confirmed by reading
`SipCall::Impl`'s member layout (`src/sip/SipCall.cpp`) rather than assumed.

**Consequence**: if pjsua2's own call-forking/early-dialog handling
ultimately maps every provisional response branch onto the same `pjsua2::Call`
object (as is standard for pjsua2's simplified call model — it does not
expose per-branch `pjsip_dlg` objects to application code, unlike the raw
PJSIP C API), then this client's MSRP session is structurally already
"whichever branch pjsua2 itself considers current for that Call" — the same
guarantee pjsua2 already provides for audio/video/RTT in this codebase. No
MSRP-specific forking bug was found, but also no MSRP-specific forking
protection was *added*, because pjsua2's own call abstraction was already
the single source of truth this client's audio/video/RTT code already
trusts.

## What was not tested

- **No live forking scenario was exercised** in this session — this would
  require either a real proxy/server that forks INVITEs (SIP-Server-RTT
  configured to fork, or a similar test fixture) or a from-scratch PJSIP
  test harness capable of driving multiple provisional responses with
  distinct to-tags into the pjsua2 layer. Neither was available. Per the
  W102 spec's own instruction ("Dacă stack-ul nu permite testare reală de
  forking, adaugă fixture-uri și raportează testul live ca NOT RUN"), this
  is reported as **NOT RUN**, not assumed to pass.
- **180/183/PRACK/UPDATE-in-early-dialog**: `onCallRxReinvite` (existing,
  W095) already handles UPDATE-carried video/RTT consent changes; whether an
  early-dialog UPDATE could also carry a peer-initiated `m=message` offer
  before the dialog is confirmed was not specifically tested — the
  offer/answer code path added in W102 (msrp-offer-answer.md) does not
  distinguish early vs. confirmed dialog state, so it would run either way,
  but this was verified only by code inspection, not a live early-dialog
  MSRP offer.
- **CANCEL / non-2xx final response cleanup**: `~SipCall()`'s existing MSRP
  session teardown (deletes `msrpSession`, releases `msrpSdpPool`) already
  runs on ordinary call teardown; whether pjsua2 itself calls the
  destructor path correctly for a CANCELed/rejected call the same way it
  does for a normal BYE was not re-verified specifically for the MSRP
  addition — this reuses the exact teardown path already exercised by
  W090-W101's own audio/video/RTT regression tests (unchanged, still
  passing).

## Conclusion

No forked-branch MSRP misattribution bug was found by design (pjsua2's
single-`Call`-object model structurally prevents the specific hazard the
task calls out — "a forked branch's abandoned MSRP connection being
associated with the winning dialog" cannot occur because there is only ever
one `SipCall`/`MsrpSession` pairing per pjsua2 `Call`), but this was
established by code audit, not by a live forking test. Live forking
scenarios are reported **NOT RUN** for this task.
