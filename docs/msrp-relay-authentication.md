# MSRP Relay Authentication (RFC 4976) — Task W107

## Standard and format source

The MSRP relay extensions were never present anywhere in this repository
before this task (confirmed by a repo-wide search — see
[agent-results/W107-msrp-relay-authentication-result.md](agent-results/W107-msrp-relay-authentication-result.md)
for the exact audit). This implementation follows **RFC 4976 "Relay
Extensions for the Message Sessions Relay Protocol (MSRP)"**:

- A new MSRP method, `AUTH`, used to reserve a session at a relay before
  that relay's URI can be offered in SDP.
- The relay challenges an unauthenticated `AUTH` with a `401`/`407` response
  carrying a `WWW-Authenticate` header — HTTP Digest (RFC 2617) syntax,
  reused with `method="AUTH"` and `digest-uri` = the relay's own MSRP URI
  (there is no SIP dialog involved in this exchange — it is a standalone
  digest handshake over the MSRP control connection itself).
- A successful `200 OK` carries a `Use-Path` header: the relay-allocated
  MSRP URI (and any further relay hops) that the client must offer as its
  own path in SDP `a=path` instead of a locally-bound address.
- `Expires` bounds how long the allocation is valid; refreshing means
  repeating the `AUTH` exchange before it lapses.

No behavior beyond what these three headers (`WWW-Authenticate`,
`Authorization`, `Use-Path`) plus `Expires` define was invented. Where the
draft is silent (exact retry counts, refresh margin), this implementation
uses caller-configurable values (see `MsrpRelayConfig`) rather than a
hardcoded guess.

## Why pjsip's/Qt's existing digest code could not be reused

Audited before writing any code (Phase 1):

- SIP REGISTER digest auth (`SipAccount.cpp`) is entirely inside pjsip's C
  library — `pj::AuthCredInfo` is handed the plaintext password and pjsip
  computes HA1/HA2/response internally. Nothing is exposed as standalone,
  reusable functions.
- XCAP digest auth (`XcapClient.cpp`) delegates to Qt's own
  `QNetworkAccessManager` challenge/response machinery — again, no exposed
  primitives.

`MsrpDigestAuth` (`src/msrp/MsrpDigestAuth.h/.cpp`) is therefore a small,
from-scratch, dependency-free implementation of RFC 2617's HA1/HA2/response
math, verified against RFC 2617 §3.5's own worked example
(`test_msrp_digest_auth.cpp`) — not against MSRP-specific behavior, since
the hashing itself is protocol-agnostic.

## Architecture

A dedicated component, **not** folded into `MsrpSession`:

- `MsrpRelayConfig` (`src/msrp/MsrpRelayConfig.h`) — `Disabled` /
  `Automatic` / `Required` mode, relay host/port/transport, a
  `credentialProfileId`/`username` pair (the actual password is loaded by
  the *caller* from `CredentialStore` and passed to
  `MsrpRelayClient::configure()` — the relay client itself never touches
  `CredentialStore`), TLS policy, timeouts, refresh margin, retry count.
  Every field defaults to empty/`Disabled` — nothing is a literal
  host/port/credential.
- `MsrpDigestAuth` (`src/msrp/MsrpDigestAuth.h/.cpp`) — pure HA1/HA2/
  response/challenge-parsing functions.
- `MsrpRelayAllocation` (`src/msrp/MsrpRelayAllocation.h`) — the result of a
  successful reservation: `usePath`, `expiresAt`, internal correlation ids.
  Only ever constructed from real relay-response bytes
  (`MsrpRelayClient::parseUsePath`), never guessed.
- `MsrpRelayDiagnosticsEvent`/`MsrpRelayDiagnosticsStore`
  (`src/msrp/MsrpRelayDiagnosticsEvent.h/.cpp`,
  `MsrpRelayDiagnosticsStore.h/.cpp`) — mirrors the existing
  `MsrpDiagnosticsEvent`/`MsrpDiagnosticsStore` pattern; every field is
  pre-redacted (see [msrp-relay-security.md](msrp-relay-security.md)).
- `MsrpRelayClient` (`src/msrp/MsrpRelayClient.h/.cpp`) — owns its own
  `MsrpTransport` (TCP or TLS, reusing the existing
  `MsrpTcpTransport`/`MsrpTlsTransport` — no new transport code needed,
  since a relay control connection is just an outbound MSRP-framed TCP/TLS
  connection like any active-connector session), drives the AUTH state
  machine, and emits `allocationReady`/`allocationRefreshed`/
  `allocationFailed`/`allocationLost`.

### State machine

```
Idle -> Connecting -> Connected -> SendingInitialAuth -> WaitingChallenge
     -> SendingAuthenticatedAuth -> WaitingAllocation -> Allocated
     -> Refreshing -> Allocated (loop)
     -> Reauthenticating -> Connecting (retry) | Failed
     -> Closing -> Closed
```

Every state is a real `MsrpRelayClient::State` enum value (`Q_ENUM`), and
`stateChanged` fires on every transition — this is what
`docs/msrp-relay-testing.md`'s tests assert against, not inferred behavior.

### AUTH challenge/response, precisely

1. `sendInitialAuth()`: an `AUTH` request with `To-Path` = the relay's own
   URI and a throwaway `From-Path` (temporary session-id, unauthenticated —
   this connection has no real session yet). No `Authorization` header.
2. Relay responds `401`/`407` with `WWW-Authenticate: Digest realm=...,
   nonce=..., algorithm=MD5, qop="auth"` (or without `qop`, handled either
   way — see `MsrpDigestAuth::buildAuthorizationHeader`'s qop-empty branch).
3. `sendAuthenticatedAuth()`: HA1 = MD5(username:realm:password), HA2 =
   MD5("AUTH":relay-uri), response = MD5(HA1:nonce:nc:cnonce:qop:HA2) (or
   MD5(HA1:nonce:HA2) when no qop was offered) — a fresh `AUTH` request with
   `Authorization: Digest ...` carrying that response, `cnonce`, and an
   8-hex-digit `nc` (nonce-count, tracked per-nonce so a refresh cycle that
   reuses the SIP-Digest-style pattern of one nonce/many requests would
   increment correctly — though in practice each `AUTH` cycle here gets its
   own fresh nonce, matching what the live relay actually did — see
   [msrp-relay-sip2sip-validation.md](msrp-relay-sip2sip-validation.md)).
4. **A `401` received while `WaitingAllocation`** (i.e. rejecting the
   *authenticated* retry, not the original unauthenticated one) is treated
   as a hard auth failure, not a new challenge to loop on forever — this
   was a real bug found and fixed during implementation (see
   `MsrpRelayClient::handleAuthFrame`'s comment) before it was caught in a
   test, then reconfirmed against the live relay.
5. `200 OK` with `Use-Path` (+ optional `Expires`, default 600s per RFC
   4976 when omitted) → `MsrpRelayAllocation`, `allocationReady` fires.
6. Refresh: `scheduleRefresh()` arms a `QTimer` for
   `expiresAt - refreshMarginSeconds` (config, default 30s) before expiry;
   firing re-drives the same `sendInitialAuth()`/challenge/response cycle
   over the *same* still-open control connection, and
   `allocationRefreshed` (not `allocationReady`) fires on success — tracked
   via `m_refreshInProgress`, a flag set before the cycle starts and
   consulted at the end (state alone can't distinguish an initial
   allocation from a refresh, since intervening transitions overwrite it).
7. Recovery: any network error, auth failure below `maxRetries`, or an
   unsolicited disconnect reopens the connection from scratch
   (`attemptRetryOrFail` → deferred `openTransport()` via
   `QTimer::singleShot(0, ...)` — deferred because `attemptRetryOrFail` is
   usually reached from *inside* the transport's own signal handler, and
   destroying that same transport synchronously mid-signal-emission is
   undefined behavior; this was caught by a real segfault during testing,
   not by inspection — see the Testing doc). Exhausting `maxRetries` calls
   `failAllocation()`, emitting `allocationFailed` (no prior allocation) or
   `allocationLost` (had one, now gone).
8. A relay that rejects an authenticated `AUTH` and then closes the TCP
   connection fires *two* Qt signals for the same root cause
   (`bytesReceived` with the 401, then `disconnected`) — `onTransportDisconnected()`
   now recognizes states already being handled
   (`Reauthenticating`/`Failed`/`Closing`/`Closed`) and does not
   double-count the retry, a second real bug found via the live relay test
   (it was silently doubling the reconnect count before this fix).

## What is deliberately *not* wired in this task (documented, not hidden)

`MsrpSession::adoptExternalTransport()` (`src/msrp/MsrpSession.h/.cpp`) is a
new, tested primitive: a session can adopt an already-connected transport
(e.g. the same TCP/TLS connection `MsrpRelayClient` used for its AUTH
handshake) instead of opening its own via `connectAsActive()`/
`listenAsPassive()` — exactly what RFC 4976 relay-assisted MSRP needs (the
control connection *is* the media connection once allocated).

What is **not** done: wiring a pre-call relay allocation into
`SipCall::Impl::startMsrpPassiveListener()`/`startMsrpActiveConnect()` so a
live outgoing/incoming call's SDP offer actually carries a relay-allocated
`a=path` instead of a local one. The reason is architectural, not effort:
`onCallSdpCreated` (where the SDP is built) is a **synchronous** pjsua2
callback, while obtaining a relay allocation is inherently an asynchronous
network round-trip — the call flow would need to pre-allocate *before*
starting the INVITE (`SipManager::makeCall` restructuring), which touches
audio/video/RTT-call-critical code that this task's own rules forbid
breaking, and which cannot be safely validated without live GUI testing.
This is called out explicitly, the same way Task W103 documented a real
blocker instead of fabricating a result, rather than silently wiring
something unverified into the live call path. See "Ce rămâne pentru W108"
in the result doc.

Because this wiring doesn't exist yet, **relay mode has zero effect on any
real call** even if configured — `MsrpRelayConfig::mode` defaults to
`Disabled` and nothing in `SipCall`/`SipManager` reads it. Direct MSRP is
therefore provably unaffected (see [msrp-relay-testing.md](msrp-relay-testing.md)).
