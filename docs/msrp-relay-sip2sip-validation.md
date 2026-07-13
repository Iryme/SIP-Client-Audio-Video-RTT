# MSRP Relay Live Validation — sip2sip.info (Task W107)

Manual live-validation run (2026-07-13) against the real, public RFC 4976
MSRP relay `msrprelay.sipthor.net:2855`, using the same disposable
sip2sip.info test account used for [sip2sip.info registration/call/XCAP
validation](sip2sip-live-validation.md) in a prior session (this branch
started from W106 and does not itself contain that earlier doc — see the
project-status.md entry for cross-reference). Credentials were supplied
directly by the project owner for this disposable test account and were
**never written to any tracked file** — passed only via environment
variables (`MSRP_RELAY_LIVE_USERNAME`/`MSRP_RELAY_LIVE_PASSWORD`) to the
throwaway `live_msrp_relay_probe` process.

## Relay discovery (no manual sip2sip-side setup needed)

```
> nslookup -type=SRV _msrps._tcp.sip2sip.info
_msrps._tcp.sip2sip.info  SRV service location:
  priority = 0, weight = 0, port = 2855
  svr hostname = msrprelay.sipthor.net
> nslookup msrprelay.sipthor.net
Address: 174.142.205.47
```

Confirms the earlier open question from the sip2sip live-validation
session: the relay is real, publicly reachable, and auto-discoverable via
standard DNS SRV — no additional account configuration on sip2sip.info's
side is required to have a relay to test against.

## Environment limitation found and fixed first (not a protocol issue)

The first run failed immediately with Qt's `"TLS initialization failed"` —
`live_msrp_relay_probe.exe` is a standalone throwaway probe run directly
from the build tree (not `windeployqt`-deployed like the shipped app), so
Qt could not locate its TLS backend plugin (`qschannelbackend.dll`/
`qopensslbackend.dll` under `<Qt>/plugins/tls`). Setting
`QT_PLUGIN_PATH=<Qt install>/plugins` for the probe process resolved it.
This is a pre-existing, already-documented gap
(`test_msrp_session_harness.cpp`'s own comment: "TLS/abort/invalid-frame-
injection scenarios... documented as a known gap in docs/msrp-testing.md")
— TLS was never exercised over the wire by any automated or prior manual
test in this project before this session; it is not something this task
introduced or broke.

## What was actually observed (real relay-response bytes)

Run: `MSRP_RELAY_LIVE_HOST=msrprelay.sipthor.net
MSRP_RELAY_LIVE_PORT=2855 MSRP_RELAY_LIVE_USERNAME=ipaulmihai
MSRP_RELAY_LIVE_TLS=1`, via `live_msrp_relay_probe`.

1. **TLS connect: PASS.** `QSslSocket` completed a real TLS handshake to
   `msrprelay.sipthor.net:2855`.
2. **Initial `AUTH` accepted and challenged: PASS.** The relay responded
   with a real `401` and a `WWW-Authenticate: Digest realm=..., nonce=...,
   algorithm=MD5, qop="auth"` header — confirmed via
   `MsrpRelayDiagnosticsStore`'s logged event
   (`kind=ChallengeReceived, responseCode=401, algorithm=MD5, qopUsed=yes`).
   This proves the relay accepts this client's `AUTH` request framing
   (start-line, `To-Path`/`From-Path`, transaction-id) as valid MSRP.
3. **Authenticated `AUTH` retry: FAIL (no response).** After computing and
   sending the digest response (`Authorization: Digest username="ipaulmihai",
   realm=..., nonce=..., uri=..., response=..., algorithm=MD5, qop=auth,
   cnonce=..., nc=00000001`), the relay never sent any response at all —
   confirmed by `m_authTimer` firing after the full 8-second timeout, three
   times in three independent reconnect cycles (`kind=Reconnect,
   error="relay AUTH response timed out"` logged for each). This is a
   clean, silent drop, not a `4xx` rejection — the relay simply stopped
   responding on that connection after the second `AUTH`.
4. **Allocation: FAIL, `Use-Path` never obtained.**

## Interpretation (a hypothesis, not asserted as fact)

The relay's willingness to challenge the *first* `AUTH` and its silence on
the *second* is consistent with either: (a) the relay expecting a different
canonicalization of the digest response than RFC 2617's standard HA1/HA2
construction (some deployed relays are known to diverge from the draft in
undocumented ways), (b) the relay expecting the authenticated retry on a
*new* TCP/TLS connection rather than reused on the same one (this
implementation reuses the connection, per a literal reading of RFC 4976),
or (c) sip2sip.info's relay requiring account-side relay authorization this
disposable test account was never granted, silently dropping instead of
rejecting. None of these could be distinguished without either a second,
cooperating real MSRP-relay-capable client, a packet capture of a *working*
client's exchange against the same relay, or direct correspondence with
the relay operator — all out of scope for this task. Reported as **FAIL**,
not **PASS**, per the task's explicit instruction not to declare relay
interoperability PASS without an observed successful `Use-Path` allocation.

## Direct MSRP vs. relay-assisted MSRP (explicit separation, per task instruction)

- **Direct MSRP** (Tasks W100–W106): live-validated end-to-end in the
  earlier sip2sip.info session referenced above (registration, audio,
  audio+video, audio+video+RTT calls) for SIP/audio/video/XCAP — MSRP
  session establishment itself was never tested with a second real peer in
  that session (explicitly called out as untested there: "sip2sip.info does
  not publish a dedicated test endpoint... meaningful validation would need
  a second real registered account/client").
- **Relay-assisted MSRP** (this task, RFC 4976): TLS connectivity and the
  initial AUTH/challenge exchange are confirmed real and working against
  msrprelay.sipthor.net; the full allocation handshake is confirmed **not**
  currently interoperable with this specific relay, for a reason not fully
  diagnosable from the client side alone. **The prior claim that "MSRP
  works, tested from the GUI" must not be read as validating this relay
  path** — that earlier manual GUI test had no way to exercise RFC 4976 at
  all (the client had zero AUTH-method support before this task), so
  whatever "worked" there was necessarily a different code path (most
  likely SIP MESSAGE fallback, or a direct-MSRP exchange with a peer that
  itself was not going through the relay).

## Evidence retained

Console output (state transitions + `MsrpRelayDiagnosticsStore` dump) from
the final run is reproduced above verbatim (kind/responseCode/algorithm/
qopUsed/error columns) — no raw packet capture tool was available in this
environment; the diagnostics store's redacted event log is the retained
evidence, consistent with what production diagnostics would show.

## Manual test status

| Scenario | Status |
|---|---|
| DNS SRV discovery of relay | PASS |
| TLS connect to relay | PASS |
| Initial AUTH / 401 challenge received | PASS |
| Authenticated AUTH / allocation | FAIL |
| SEND through relay | BLOCKED (no allocation) |
| Refresh / reconnect / two simultaneous sessions | NOT RUN (blocked by allocation failure) |
| SIP-Server-RTT relay test | NOT RUN (separate repo, not available in this session) |
