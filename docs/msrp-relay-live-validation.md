# MSRP Relay Live Validation (Task W108)

Strict status vocabulary used below: **PASS / FAIL / BLOCKED / NOT RUN /
UNSUPPORTED**. No relay interoperability is ever declared PASS without
direct evidence (a successful allocation *and* real wire bytes *and* a real
dialog/SEND).

## sip2sip.info relay (`msrprelay.sipthor.net:2855`)

**Status: BLOCKED.** This session had no relay test credentials available
— the W107 session's test account password
(`MSRP_RELAY_LIVE_PASSWORD` environment variable, per
[msrp-relay-security.md](msrp-relay-security.md)'s "never commit a
credential" rule) was never persisted anywhere this session could read it,
and no new credential was supplied. Every item below that requires a live
relay round-trip is therefore **NOT RUN**, not FAIL — the code was not
exercised against real infrastructure this session, so no interoperability
conclusion (positive or negative) can be drawn.

| # | Scenario | Status |
|---|---|---|
| 1-5 | DNS-SRV / TLS connect / initial AUTH / 401 challenge / authenticated AUTH | NOT RUN this session (W107 already recorded 1-4 as PASS, 5 as FAIL — see below) |
| 6 | Allocation | NOT RUN this session |
| 7 | Path valid | NOT RUN |
| 8-9 | Outbound call preparation / INVITE with relay path | NOT RUN |
| 10 | Answer with m=message | NOT RUN (inbound relay is also NOT IMPLEMENTED — see [msrp-relay-live-call-integration.md](msrp-relay-live-call-integration.md)) |
| 11 | Transport adoption (real call) | NOT RUN (unit-tested with a scripted local relay only — see that doc) |
| 12-16 | SEND / MSRP response / REPORT / CPIM / IMDN / is-composing over relay | NOT RUN |
| 17 | Refresh | NOT RUN live; unit-tested against a scripted relay (W107) |
| 18 | Recovery | NOT RUN live; unit-tested (W107) |
| 19 | Call termination cleanup | NOT RUN |

## Direct MSRP

**Status: PASS (regression only, via automated tests)** — the full
W090–W108 suite (75/75 `ctest` targets, including
`test_msrp_session_harness`'s real loopback SEND/response exchange) passes
unmodified. **Not live-retested against a real third-party peer this
session** (no live SIP account/peer available) — this is the same
limitation [msrp-testing.md](msrp-testing.md) already documents for prior
tasks, not a new gap introduced by W108. W108's own code changes to the
direct-MSRP path are limited to: `onCallSdpCreated`'s offerer branch now
checks `preparedMsrpOffer.mode == Relay` *before* falling through to the
unchanged `startMsrpPassiveListener()` call — when no relay offer is
prepared (the default, `Disabled` mode), the direct-MSRP branch executes
exactly as it did before W108, byte-for-byte.

## Audio / Video / RTT

**Status: PASS (regression only)** — same caveat: the automated suite
(which includes call-state-machine, codec negotiation, video pipeline, and
RTT foundation tests) passes unmodified, and W108's changes to
`SipManager`/`SipCall` are gated entirely behind `AppSettings::msrpRelayMode()`
defaulting to `"disabled"` (see
[msrp-relay-live-call-integration.md](msrp-relay-live-call-integration.md)),
so no code path exercised by an audio/video/RTT-only call (or any call with
relay disabled) changed at all. **Not live-retested with a real GUI call
this session** — no interactive testing environment was available.

## Phase 15 — Investigating the W107 "authenticated AUTH gets no response" failure

W107 recorded: TLS connect and the *initial, unauthenticated* AUTH/401
challenge succeeded against the real sip2sip.info relay, but the
*authenticated* AUTH retry received no response at all (silent drop, not a
4xx) across three attempts.

**This session could not reproduce or further probe that failure** (no
credentials — see above), so this is a static code review only, not a
capture-based diagnosis. Findings:

- `MsrpRelayClient::digestUri()` (`src/msrp/MsrpRelayClient.cpp`) returns
  `msrp://<host>:<port>` (no session-id, no trailing slash) as the digest
  `uri=` parameter, and `relayToPathHeader()` appends `/;tcp` to build the
  wire To-Path: `msrp://<host>:<port>/;tcp` — an MSRP URI with an *empty*
  session-id segment. This is used identically for both the initial
  unauthenticated AUTH and the authenticated retry, so it cannot by itself
  explain why only the *second* request gets no response.
- The authenticated request's `Authorization` header is built by
  `MsrpDigestAuth::buildAuthorizationHeader()` (verified independently
  against RFC 2617's own worked example in `test_msrp_digest_auth.cpp`),
  using `HA2 = MD5(method:uri)` with `method="AUTH"` and the same
  `digestUri()` value used for the To-Path — consistent with RFC 4976 §5.2
  as understood from this codebase's own documentation, but **not
  cross-checked against a second independent RFC 4976 implementation or a
  raw packet capture**, since neither was available this session.
- A fresh, unique `transaction-id` and a fresh `From-Path` temp session-id
  are used only once per connection (`m_localTempSessionId`, set in
  `sendInitialAuth()` and reused unchanged for the authenticated retry on
  the same connection) — this matches typical MSRP practice (From-Path
  identifies the connection's temporary identity, not each request) but is
  likewise unverified against a second implementation.

**Hypotheses recorded by W107, still unconfirmed after this review:**
1. A digest canonicalization mismatch (unlikely to be `HA1`/`HA2`
   themselves, since those are RFC-2617-vector-verified in isolation, but
   possibly the exact `uri=` string the relay expects to see restated in
   `Authorization` vs. what this client sends).
2. The relay expects a *new* TCP/TLS connection for the authenticated
   retry rather than reusing the same connection the challenge arrived on.
3. The test account is not authorized/provisioned for relay use at all
   (only for direct registration/calling).

**Recommended next step** (not performed this session): capture the raw
TCP/TLS bytes of both AUTH requests and the (absent) response with a
packet capture tool while re-running `live_msrp_relay_probe`
(`tests/live_msrp_relay_probe.cpp`, Task W107) against the real relay with
valid credentials, then diff the authenticated request's exact bytes
against RFC 4976 §5.2's example. This requires live credentials this
session did not have access to.

## Confirmation

No relay interoperability item above is declared PASS without direct
evidence of a successful allocation, real SDP wire bytes, and a real
dialog/SEND — every relay-dependent row in the tables above is BLOCKED or
NOT RUN, consistent with what was actually exercised this session.
