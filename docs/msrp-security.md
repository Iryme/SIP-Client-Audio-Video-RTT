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

## Inbound connection authentication (Task W105)

`MsrpTcpTransport::listenPassive()` accepts the first TCP connection it
receives unconditionally and then closes the listener (RFC 4975's passive
role has no lower-layer authentication of its own — this matches the spec,
not a bug in the transport). Before Task W105, `MsrpSession::handleFrame()`
processed any inbound SEND/REPORT request from that connection without
checking who it actually came from, so any local process able to connect to
the ephemeral listening port before the real, SDP-negotiated peer would be
silently trusted and could inject messages into the session.

Per RFC 4975 §7.1, `MsrpSession::toPathTargetsThisSession()` now checks that
every inbound *request*'s `To-Path` last URI's session-id equals this
session's own negotiated local session-id before it is processed further. A
mismatch is rejected with a `403 Forbidden` response, logged as an
`MsrpDiagnosticsEvent` (`Kind::Error`), and never reaches
`payloadReceived`/`fileTransferReceived` or the chunk assembler. The
session-id is the one part of the URI an attacker cannot guess without
having already observed the SDP `a=path` exchanged over the signaling
channel — host/port alone (which a local port-scan could discover) are not
sufficient to pass this check.

This does not change behavior for a legitimate peer: the To-Path a real
peer sends is always built from the `a=path` this client advertised in its
own SDP offer/answer, so it always matches.

## Connection-hijack denial-of-service recovery (Task W106)

Task W105 stopped a rogue local connection from injecting messages, but the
underlying denial-of-service was still open: `MsrpTcpTransport`/
`MsrpTlsTransport` stop listening for good once they accept their first TCP
connection (see `onNewConnection`), so a rogue process that merely won the
accept race — without ever sending a valid request — could permanently
prevent the real, SDP-negotiated peer from connecting at all, even though
none of its messages would have been accepted.

`MsrpSession` now tracks whether it has ever accepted one authenticated
inbound request (`m_awaitingAuthentication`). While that is still true,
`rejectUnauthorizedRequest()` calls `MsrpTransport::relisten()`: the
rejected connection is dropped without treating the session as closed or
failed, and the listener reopens on the exact same bind address/port for
whatever time remains of the original accept window
(`msrpAcceptTimeoutMs`/`msrpConnectionTimeoutMs`), giving the real peer a
further chance to connect. Once a request actually passes the To-Path
check, `m_awaitingAuthentication` is cleared and later disconnects are
handled by the normal close/error path — an already-established, validated
session is never torn down and relistened on.

This is bounded by the original accept window: if it elapses before a
valid peer connects, `relisten()` surfaces the same `accept timeout: no
peer connected` error as an ordinary failed passive listen.

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
  limiter — a rogue peer can still repeatedly reconnect-and-fail within a
  single session's accept window (see Task W106 above), consuming CPU for
  parse/reject cycles, even though it can never inject messages or
  permanently deny the real peer.
- Stalled-connection detection relies on `msrpIdleTimeoutSeconds` /
  `msrpTransactionTimeoutMs` being enforced by the owning `MsrpSession` /
  `MsrpChunkAssembler::purgeStale` — there is no separate TCP-level
  keep-alive/heartbeat mechanism.
- Server-side TLS certificate/key provisioning has no production UI (see
  [msrp-transport.md](msrp-transport.md)) — only usable in the local test
  harness context.

## MSRP relay (RFC 4976) — Task W107

Relay-assisted MSRP has its own dedicated security notes — see
[msrp-relay-security.md](msrp-relay-security.md) — since it introduces a
new credential-handling path (relay AUTH digest) distinct from everything
above. Relay support is `Disabled` by default. As of W108, an *outbound* call can
opt into relay-assisted MSRP (still off by default —
`AppSettings::msrpRelayMode()` defaults to `"disabled"`) — see
[msrp-relay-live-call-integration.md](msrp-relay-live-call-integration.md).
This does not change this file's threat model for direct MSRP: the relay
decision is fully resolved before a call exists, and when relay is
disabled/not configured, `onCallSdpCreated` takes the exact same direct-MSRP
code path described above.
