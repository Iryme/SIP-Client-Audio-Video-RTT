# MSRP Offer/Answer (Task W102)

Covers `SipCall::Impl::PjCall::onCallSdpCreated`'s answer branch
(`src/sip/SipCall.cpp`) and `MsrpSipMediaInjector::answerMessageMediaAtIndex`
(`src/msrp/MsrpSipMediaInjector.{h,cpp}`). See
[msrp-live-sdp-integration.md](msrp-live-sdp-integration.md) for the
offerer path this builds on.

## Why the answer path needed a separate mechanism

`onCallSdpCreated` fires for **both** the offer this client sends and the
answer this client sends back to a peer-initiated offer — `prm.remSdp` is
empty in the first case, populated in the second (this is the
already-established offerer/answerer detection from W101).

By the time the callback fires for an answer, PJSIP's own SDP negotiator
(`pjmedia_sdp_neg.c`, `create_answer()`/`match_offer()`) has already built
`prm.sdp` by walking the offer's media list and, for each section, looking
for a matching *local* media capability. Since PJSUA's media manager has no
concept of the `"message"` media type, that lookup always fails for
`m=message`, and `create_answer()` falls into its own
"no matching media... clone-and-deactivate" branch — producing a **rejected
`m=message 0 ...` placeholder at the same index** as the offer's section,
automatically, before our callback ever runs. This was verified by reading
`pjmedia_sdp_neg.c` directly (see `match_offer`/`create_answer`, lines
~1277-1813) rather than assumed.

This means two things:

1. **We never have to construct the "reject" case ourselves for the answer
   path** — if we do nothing, PJSIP already sends a valid, spec-compliant
   rejection.
2. **To accept, we must *replace* the section at that exact index**, not
   append one (appending would violate RFC 3264 §6's "same number of media
   descriptions, same order" rule for answers and desynchronize every
   later media index against the offer).

## `MsrpSipMediaInjector::answerMessageMediaAtIndex`

Same `pjSdpSession->media[i]` in-place mutation technique as the offerer
path (see msrp-live-sdp-integration.md — safe because
`Endpoint::on_call_sdp_created` only re-parses `wholeSdp` *text* if it
changed, never touches `pjSdpSession` itself), but writes to
`sdp->media[index]` instead of `sdp->media[sdp->media_count++]`. Refuses to
run (returns `injected=false`, leaves the session untouched) if:

- `index` is out of range, or
- the media section already at that index isn't literally `"message"`.

This is a defensive check against ever answering into the wrong slot if a
future change in call flow changes what index MSRP lands at.

Proven at the wire-bytes level in `tests/test_msrp_sip_media_injector.cpp`
(`answersAtCorrectIndexReplacingRejectedPlaceholder`,
`answerRejectsWrongIndex`, `answerRejectsOutOfRangeIndex`) — parses a
fixture that mirrors PJSIP's real rejected-placeholder shape, replaces it,
and asserts `pjmedia_sdp_print()` output.

## Extracting the offer's `m=message` sections

`SipCall.cpp`'s `extractRemoteMessageBlocks()` walks `prm.remSdp.pjSdpSession`
for `"message"` media sections, prints each one with the public
`pjmedia_sdp_media_print()`, and feeds that text into the **existing,
already-tested** `MsrpSdpNegotiator::parseMessageBlocks()` text parser (W100)
— reused as-is, not re-implemented, per the project rule against duplicating
parsers. This gives, per section: index, rejected/port, transport, `a=path`
(parsed into `QList<MsrpUri>`), `a=setup`, accept-types/accept-wrapped-types.

## Deciding whether to accept

A peer-offered `m=message` section is accepted only if:

- it isn't already rejected (`port != 0`) in the offer, and
- `AppSettings::enableMsrp()` and (`enableMsrpTcp()` or `enableMsrpTls()`)
  are on locally.

Otherwise PJSIP's own port-0 rejection is left exactly as-is — MSRP being
disabled locally never causes the call itself (or its audio/video/RTT
sections) to be rejected; only the `m=message` section is.

## Multiple simultaneous `m=message` offers

`SipCall` owns exactly one live `MsrpSession` per call (an architectural
decision from W101, unchanged here). If a peer's SDP contains more than one
non-rejected `m=message` section, only the first is answered/accepted; every
additional one is explicitly logged and left rejected (port 0) — **not**
silently dropped or multiplexed onto the one session, which would risk
mis-attributing frames between two independent MSRP conversations. This is a
documented limitation, tracked for W103 (see the final report).

## Renegotiation (re-INVITE / UPDATE carrying a peer m=message)

If `m_impl->msrpSession` already exists (an MSRP session from an earlier
negotiation in the same call), a later peer-initiated re-offer reuses that
same session/role rather than creating a second one — it is simply
re-answered at whatever index the new offer places `m=message` at (indices
can shift if the peer reorders media across a renegotiation).

## What is still explicitly not covered

- Only the offer/answer *shape* (accept vs. reject at the right index,
  right role) is handled for the peer-initiated path; a full
  hold/resume/remove/reintroduce state-machine walk (Task W102's Faza 2
  list) was exercised only through unit fixtures of the SDP shapes
  involved, not through a live re-INVITE sequence against a real dialog
  (no PJSIP call fixture harness exists in this codebase for driving a full
  in-process INVITE/re-INVITE/UPDATE transaction sequence) — see
  [msrp-live-interoperability.md](msrp-live-interoperability.md) for exactly
  what was and wasn't exercised live.
