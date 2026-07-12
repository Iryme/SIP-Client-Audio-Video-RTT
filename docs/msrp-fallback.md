# MSRP Transport Policy & Fallback (Task W101, Phases 5 & 7)

## Wiring (Phase 5)

`SipManager::sendSipMessage()` (`src/sip/SipManager.cpp`) is the single
choke point for every composer-originated outbound message. It now calls
`MessagingTransportPolicy::decideInitialTransport(mode, msrpCandidate,
allowFallback)` before doing anything transport-specific:

- `mode` — `AppSettings::messagingTransportMode()` (`sip-message-only` /
  `msrp-preferred` / `msrp-required` / `automatic`).
- `msrpCandidate` — `true` only when there is a currently active call
  (`m_activeCall`, single-active-call model), that call's own
  `MsrpSession` has actually reached `Established` (`SipCall::
  isMsrpEstablished()` — never inferred from SDP negotiation alone), *and*
  that call's remote URI matches the message's destination.
- `allowFallback` — `AppSettings::allowSipMessageFallback()`.

If the decision selects MSRP, `SipCall::sendMsrpMessage()` is called
directly (no SIP MESSAGE is composed at all for that attempt — no
dual-send). If the MSRP send call itself fails (empty Message-ID
returned — e.g. the session dropped between the established check and the
send), `MessagingTransportPolicy::decideFallbackAfterMsrpFailure()` decides
whether a SIP MESSAGE retry is permitted; `msrp-required` mode never
falls back (returns a hard error instead), matching the pure decision
logic already unit-tested in `test_messaging_transport_policy`
(unchanged from Task W100).

## Automatic recovery (Phase 7)

There is no persistent/sticky "we fell back, stay on SIP MESSAGE" state.
`msrpCandidate` is recomputed **fresh on every call** to `sendSipMessage`
by checking the live session state at that instant. So if MSRP becomes
established again after an earlier message fell back (e.g. the call's
listener finishes its handshake a moment later), the very next outbound
message automatically routes over MSRP again — with no call restart and
no extra bookkeeping needed. This satisfies the task's explicit
requirement ("dacă MSRP devine din nou disponibil, transportul trebuie să
poată reveni automat pe MSRP fără restartul apelului") as a direct
consequence of the design, not as separately-implemented recovery logic.

## Delivery loop (Phase 6)

`MsrpSession::messageDeliveryStatusChanged` fires once a SEND's outcome is
known (from the response itself when no REPORT was requested, or from a
subsequent REPORT — never both, avoiding double-reporting). `SipCall`
relays this and `payloadReceived` as its own signals
(`msrpDeliveryStatusChanged` / `msrpPayloadReceived`), and `SipManager`
wires the inbound case into `MessageHistoryStore::appendInbound` (same
dedup as inbound SIP MESSAGE). The delivery-status case is currently only
logged, not correlated into `MessageHistoryStore`'s `deliveryState` — see
the commit message for `feat(msrp): close the send-response-report-history
loop` and the "What remains" section of the final report for why.

## Covered fallback cases

Exercised via the decision function's existing unit tests
(`test_messaging_transport_policy`) plus the new live wiring:

| Case | Behavior |
|---|---|
| MSRP unavailable (no active call / not established) | `msrpCandidate=false` → falls to SIP MESSAGE (if `allowFallback`) or is rejected (`msrp-required`) |
| Session not yet confirmed (still `Connecting`) | `isMsrpEstablished()` false until `Established` — same as unavailable |
| Socket closed mid-call | `MsrpSession` transitions to `Closed`/`Failed`; next send re-evaluates and falls back |
| MSRP SEND itself fails | `decideFallbackAfterMsrpFailure` gate, no dual-send |
| `m=message` rejected (port 0) | Listener never started/established → `msrpCandidate=false` |
