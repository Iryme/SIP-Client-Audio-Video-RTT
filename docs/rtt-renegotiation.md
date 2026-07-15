# RTT Renegotiation Fix — Summary (Task W109A)

See [rtt-offer-answer-state-machine.md](rtt-offer-answer-state-machine.md)
for the full state-machine design and [rtp-port-range-configuration.md](rtp-port-range-configuration.md)
for the port-range fix. This document is the short version: what was
observed, what the root causes actually were, and what changed.

## Reproduction (baseline, pre-fix)

Two instances of this app on the same Windows machine, both using pjsua2's
default (unbounded, effectively shared) RTP base port:

```
Alice: audio call to Bob — succeeds
Bob:   requests RTT
Alice: re-INVITE offer received: callId=0 video=no text=yes
Alice: Incoming text request pending: protocol=RTT — declined auto-response, awaiting user accept
       SDP offer/answer:  m=audio 4000 / m=message 0 / m=text 0
Alice: clicks "Accept RTT"
Alice: Accepting text request: protocol=RTT
Alice: Request RTT ON
Alice: Accept RTT: textCount=1
Alice: RTP socket bind() at 0.0.0.0:4002 error: Address already in use (WSAEADDRINUSE)
       (then 4004, 4006, 4008, 4010 — pjsua2's own internal retry)
Alice: rtt=disabled
Bob:   retransmits the RTT request
```

## Root causes (two, both fixed)

1. **RTP port collision.** `pj::EpConfig` never set a media/RTP port range;
   the actual pjsua2 knob for this is per-account
   (`AccountMediaConfig::transportConfig.port`/`.portRange`), and it was
   never touched at all. Every instance on the host started probing from
   the same base port. Fixed by `RtpPortRangeConfig` (config/CLI-driven,
   applied in `SipAccount::startRegistration()`).
2. **No distinction between "local request" and "accept a remote offer".**
   `SipCall::requestRtt(true)` was reused for both, with a single in-flight
   flag and no guard, so the UI's "still pending" state could contradict
   what had already been sent over the wire (`m=text 0`). Fixed by adding
   `acceptIncomingRttRequest()`/`rejectIncomingRttRequest()` as distinct,
   guarded entry points and extending `RttSession`'s state machine (see the
   state-machine doc).

Once (1) is fixed, the very first Accept re-INVITE binds its RTP/RTCP pair
on the first try — no PJSIP-internal port probing is needed at all. (2)
independently fixes the contradictory-state bug even when a collision does
occur (e.g. misconfigured ranges): the failure is now reported as `Failed`
with a classified reason, and Reject is a real, distinct action instead of
a dialog dismiss that leaves state inconsistent.

## Files changed

- `src/sip/RtpPortRangeConfig.h/.cpp` (new) — range struct, validation,
  session-override/AppSettings/default resolution.
- `src/sip/RtpPortDiagnostics.h/.cpp` (new) — classifies bind-failure
  reasons.
- `src/sip/SipAccount.cpp` — applies the effective range before
  `account->create()`.
- `src/core/AppSettings.h` — `rtpPortRangeStart()`/`rtpPortRangeEnd()`,
  `setConfigDirectoryOverride()` (multi-instance support).
- `src/sip/SipProfileManager.h/.cpp` — `setConfigDirectoryOverride()`
  (separate profile store per instance).
- `main.cpp` — `--config-dir`/`--rtp-port-start`/`--rtp-port-end` (+
  `SIPCLIENT_CONFIG_DIR`/`SIPCLIENT_RTP_PORT_START`/`SIPCLIENT_RTP_PORT_END`
  env vars).
- `src/sip/SipCall.h/.cpp` — `acceptIncomingRttRequest()`/
  `rejectIncomingRttRequest()`/`hasPendingIncomingRttRequest()`, new
  signals (`rttRequestRejected`, `rttNegotiationFailed`, `rttLocalOfferSent`),
  own-offer-completion detection in `onCallMediaState()`.
- `src/sip/SipManager.h/.cpp` — `acceptIncomingRtt()`/`rejectIncomingRtt()`,
  signal relays.
- `src/rtt/RttSession.h/.cpp` — extended state machine, negotiation
  timeout.
- `src/gui/dialogs/MediaRequestDialog.cpp`, `src/gui/MainWindow.cpp`,
  `src/gui/panels/RttPanel.cpp` — wired to the new methods/states.
- Tests: `tests/test_rtt_session.cpp` (extended),
  `tests/test_rtp_port_range_config.cpp` (new),
  `tests/test_rtp_port_diagnostics.cpp` (new).

## What did NOT change

- Audio/video negotiation logic and codec handling.
- MSRP (`m=message`) offer/answer path (`onCallSdpCreated`) — untouched;
  RTT lives entirely in `onCallRxReinvite`/`requestRtt` and is already
  structurally independent of MSRP's media index handling.
- pjproject sources — not modified.
