# sip2sip.info Live Validation

Manual live-validation session (2026-07-13) against the public SIP2SIP test
service (`sip2sip.info`, backed by SIP Thor at `proxy.sipthor.net`) and its
XCAP server (`xcap.sipthor.net`). Unlike [pjsip-audio-call-validation.md](pjsip-audio-call-validation.md)
and [pjsip-video-validation.md](pjsip-video-validation.md), which validate
against a local lab Kamailio server, this session validates against real
public-internet infrastructure outside this project's control, using a
disposable test account. No source code behavior changed as a result of
this session — findings below are entirely about correct client
configuration and new/fixed manual test tooling.

## Account / configuration used

- SIP address: `sip:ipaulmihai@sip2sip.info`
- Domain/registrar: `sip2sip.info` (per SIP2SIP's own requirement — see
  "Configuration lesson" below — do **not** register directly against
  `proxy.sipthor.net`)
- Outbound proxy: `proxy.sipthor.net` (routing-only; see below)
- XCAP root: `https://xcap.sipthor.net/xcap-root`
- Credentials: provided by the project owner for this disposable test
  account only; never written to any tracked file — supplied via
  environment variables to the live probes and stored only in Windows
  Credential Manager for the duration of testing, the same mechanism the
  app itself uses (`CredentialStore`).

## Configuration lesson (tooling bug, now fixed)

`sip2sip.info`'s own documentation states SIP devices must resolve the
domain via NAPTR/SRV per RFC 3263 — `_sip._udp.sip2sip.info` SRV resolves
to `proxy.sipthor.net`. The correct client configuration is therefore:

- **Registrar / Request-URI host**: `sip2sip.info` (the served domain)
- **Outbound proxy**: `proxy.sipthor.net`, added as a routing-only hop
  (`config.sipConfig.proxies`, see `SipAccount.cpp`) — this adds a `Route`
  header but never rewrites the Request-URI.

`tests/live_registration_probe.cpp` had no outbound-proxy support at all
and was initially pointed directly at `proxy.sipthor.net` as the registrar,
producing `REGISTER sip:proxy.sipthor.net`. SIP Thor silently drops a
REGISTER whose Request-URI host isn't a domain it's the registrar for
(confirmed by replaying a hand-crafted raw UDP REGISTER with the same
malformed Request-URI outside the app entirely — still no response, while
a generic `OPTIONS` to the same host always got an instant `200 OK`).
This was a test-harness configuration bug, not an application bug —
`SipAccount.cpp`'s existing `outboundProxy`/`proxy` handling was already
correct (`live_audio_call_probe.cpp` had it wired correctly from the
start). Fixed by adding `SIP_LIVE_OUTBOUND_PROXY` support to
`live_registration_probe.cpp` to match.

## Live Validation Results

### Registration (2026-07-13, PASS)

`live_registration_probe` (`SIP_LIVE_SERVER=sip2sip.info`,
`SIP_LIVE_DOMAIN=sip2sip.info`, `SIP_LIVE_OUTBOUND_PROXY=proxy.sipthor.net`):
REGISTER → `200 OK`. Refresh/re-REGISTER fired and returned `200 OK`.
UNREGISTER returned `200 OK`, state machine reached Unregistered cleanly.

### Audio call — echo test (2026-07-13, PASS)

`live_audio_call_probe` against `4444@sip2sip.info` (SIP2SIP's published
microphone/echo test extension): INVITE → `180 Ringing` → `200 OK` → audio
media connected → call reached Active → media stayed up through the
validation window → clean BYE/hangup → Idle → clean UNREGISTER.

### Audio + Video call — A/V test (2026-07-13, PASS)

Same probe against `3333@sip2sip.info` (SIP2SIP's published audio/video
test extension), with `SIP_LIVE_CALL_TYPE=VIDEO` (new — see below):
`Initial call media offer: audio=yes video=yes`. Video negotiated
`ENCODING_DECODING (sendrecv)`, both `audioMediaConnected` and
`videoMediaConnected` fired, call reached Active with both media types
active simultaneously. Clean hangup/unregister.

### Audio + Video + RTT call — graceful RTT-unsupported fallback (2026-07-13, PASS)

Same target, `SIP_LIVE_CALL_TYPE=VIDEORTT`: `m=text` (RTT) was offered
alongside audio/video. The peer does not support RTT, so
`rttMediaConnected` never fired — but this did not affect audio or video:
both stayed active, the call stayed in the Active state, and hangup/
unregister completed cleanly. Confirms the app degrades correctly when a
real peer doesn't support all requested media types instead of failing
the whole call.

### XCAP GET — capabilities document (2026-07-13, PASS)

New `live_xcap_probe` (see below) against
`https://xcap.sipthor.net/xcap-root/xcap-caps/global/index`: real
`200 OK` over HTTPS, full `application/xcap-caps+xml` body returned
listing the server's supported AUIDs (`resource-lists`, `pres-rules`,
`rls-services`, `org.openmobilealliance.xcap-directory`, etc.). First-ever
live confirmation of `XcapClient`'s HTTP GET path against a real XCAP
server (previous XCAP work — Task W099 and its successors — was validated
only with unit tests against synthetic responses).

### XCAP GET — per-user document, Digest auth (2026-07-13, PASS — transport; 404 expected)

Same probe, `resource-lists/users/sip:ipaulmihai@sip2sip.info/index`: real
`404 Not Found` — this test account has never used the contact-list
feature, so the document legitimately doesn't exist. What matters for
validation purposes: the per-user path was built correctly, the request
reached the server, and Qt's Digest challenge/response
(`XcapClient::onAuthenticationRequired`) completed successfully (a wrong
password or broken auth wiring would have produced `401`, not `404`).

## Tooling changes made during this session

- `tests/CMakeLists.txt`: fixed a pre-existing build break in all three
  `live_*_probe` targets (`live_registration_probe`, `live_audio_call_probe`,
  `live_emergency_call_probe`) — none of them linked `PjsipTraceModule.cpp`/
  `SipRawMessageParser.cpp`, so they failed to build against the current
  `SipManager.cpp` (which now calls `PjsipTraceModule::install()`/
  `uninstall()`). Added `${PJSIP_TRACE_MODULE_SOURCES}` and switched the
  individual `SipTraceLogger.cpp` line to `${SIP_TRACE_SOURCES}` (which also
  pulls in `SipRawMessageParser.cpp`) in all three targets.
- `tests/live_registration_probe.cpp`: added `SIP_LIVE_TRANSPORT` (UDP/TCP)
  and `SIP_LIVE_OUTBOUND_PROXY` environment variable support (previously
  UDP-only, no outbound-proxy support at all — see "Configuration lesson").
- `tests/live_audio_call_probe.cpp`: added `SIP_LIVE_CALL_TYPE`
  (`AUDIO`/`VIDEO`/`RTT`/`VIDEORTT`) environment variable, using the
  existing `CallMediaOptions::fromType()` — previously always placed an
  audio-only call, so live video/RTT could never be exercised through this
  probe. Also reports `videoMediaConnected`/`rttMediaConnected` state.
- `tests/live_xcap_probe.cpp` (new): a minimal live probe for
  `XcapClient::get()` — stores a password via `CredentialStore` under the
  fixed `"xcap"` pseudo-profile (matching `XcapClient`'s existing
  convention), issues one GET, and prints the real HTTP response
  (status/content-type/body preview/warnings). Reads
  `XCAP_LIVE_ROOT`/`XCAP_LIVE_USERNAME`/`XCAP_LIVE_PASSWORD`/`XCAP_LIVE_XUI`/
  `XCAP_LIVE_AUID`/`XCAP_LIVE_DOCUMENT` from the environment. No PJSIP
  dependency — pure Qt/Network — but grouped in `CMakeLists.txt` next to
  the other live probes under `BUILD_LIVE_VALIDATION_TOOLS`.

None of these changes alter any shipped application behavior — they are
all confined to `tests/` manual validation tooling.

## What this session did not test

- MSRP session establishment, SIP MESSAGE instant messaging, and presence
  SUBSCRIBE/NOTIFY were not tested: sip2sip.info does not publish a
  dedicated test endpoint for any of these, and meaningful validation
  would need a second real registered account/client to exchange traffic
  with — a single test account calling itself would not exercise the real
  negotiation path with an independent peer.
- No emergency-call testing was performed against sip2sip.info (not
  applicable — `live_emergency_call_probe` targets a different, lab-only
  scenario).
