# MSRP Security (Task W100)

## Disabled by default

`AppSettings::enableMsrp()` defaults `false`; TCP/TLS sub-flags
(`enableMsrpTcp`/`enableMsrpTls`) are independently off. No MSRP socket is
ever opened, no listener ever bound, until MSRP is explicitly enabled from
the UI/config.

## TLS

- `msrpTlsVerifyPeer` defaults `true`. `MsrpTlsTransport` only calls
  `QSslSocket::ignoreSslErrors()` when this is explicitly set to `false` —
  never silently on a certificate error. Every `sslErrors` occurrence is
  surfaced via the `tlsErrors` signal and recorded as an
  `MsrpDiagnosticsEvent` error, visible on the MSRP page.
- SNI/hostname verification is handled by
  `QSslSocket::connectToHostEncrypted(host, port, host)` — the peer
  certificate is checked against the same hostname used to connect.
- `msrpTlsCaPath` optionally adds trusted CA certificates from a PEM file;
  no certificate/private key material is ever written to
  `AppSettings`/the INI file, logs, or the JSON/TXT export.
- Disabling verification is intended for a local/self-signed test
  environment only (see [msrp-testing.md](msrp-testing.md)) — it is a
  config-gated, explicit opt-in, never a default.

## Frame/message size limits

- `msrpMaxFrameBytes` (`MsrpFrameParser`'s `maxFrameBytes` constructor
  argument) bounds a single frame's total wire size; exceeding it before
  the end-delimiter is found yields `LimitExceeded` rather than unbounded
  buffering.
- `msrpMaxMessageBytes` (`MsrpChunkAssembler`'s `maxMessageBytes`) bounds a
  fully-reassembled chunked message, checked against both the chunk's
  Byte-Range `total` (rejected immediately if the declared total already
  exceeds the limit) and the running assembled size (rejected before the
  buffer is grown past the limit) — a malicious/broken peer declaring an
  enormous `Byte-Range` total can never force a large allocation.
- `msrpMaxConcurrentSessions` bounds how many MSRP sessions the UI/manual
  test flow will allow at once (enforced at the MSRP page /
  `MsrpSessionStore` level).

## Header/path injection

- `MsrpFrameSerializer::serialize()` rejects (returns empty, `ok=false`)
  any frame whose header values (including `unknownHeaders`) contain `\r`,
  `\n`, or a NUL byte — the only way this codebase ever writes a header
  line is through this single validated path.
- `MsrpPath::parseUri()`/`parsePath()` reject malformed/empty
  scheme/host/session-id/transport rather than passing them through.

## Redaction

MSRP paths carry host/port/session-id chains that can reveal network
topology or (if a session-id were ever derived from user data, which it
never is here — see `MsrpPath::generateSessionId()`) identity. Every
diagnostics/export surface redacts them:

- `MsrpDiagnosticsEvent::toPathRedacted`/`fromPathRedacted` — built by
  `MsrpSession::logDiagnostic()`'s local `redactPath()` helper, which
  reduces each path entry to just its scheme (`"msrp://…"`/`"msrps://…"`),
  dropping host/port/session-id entirely.
- The Interop JSON export's `msrpSessions`/`msrpEvents` arrays only ever
  carry the redacted transport label, never the raw path.
- Authorization-style credentials do not exist in the MSRP protocol itself
  (no `Authorization` header defined by RFC 4975) — the general rule
  ("never log Authorization/Proxy-Authorization/passwords/keys/private
  certificates") is inherited from the rest of this codebase and has no
  MSRP-specific counterpart to violate.

## What is NOT hardened at this foundation stage

- Connection-flood / many-simultaneous-listener protection beyond
  `msrpMaxConcurrentSessions` is not implemented as a dedicated rate
  limiter.
- Stalled-connection detection relies on `msrpIdleTimeoutSeconds` /
  `msrpTransactionTimeoutMs` being enforced by the owning `MsrpSession` /
  `MsrpChunkAssembler::purgeStale` — there is no separate TCP-level
  keep-alive/heartbeat mechanism.
- Server-side TLS certificate/key provisioning has no production UI (see
  [msrp-transport.md](msrp-transport.md)) — only usable in the local test
  harness context.
