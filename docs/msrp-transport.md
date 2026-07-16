# MSRP Transport (Task W100)

Covers `MsrpTransport`/`MsrpTcpTransport`/`MsrpTlsTransport`. See
[msrp-foundation.md](msrp-foundation.md) for the overall architecture.

> **Task W101 update**: `SipCall` now owns one real, call-scoped
> `MsrpSession` (and therefore one real listener) per call, started before
> the SDP offer that advertises it — see
> [msrp-live-sdp-integration.md](msrp-live-sdp-integration.md) and
> [msrp-peer-association.md](msrp-peer-association.md). The transport
> classes themselves are unchanged from W100.
>
> **Task W102 update**: `MsrpSession::connectAsActive` is now actually
> invoked for peer-initiated offers that negotiate this client into the
> active (connecting) role — see
> [msrp-offer-answer.md](msrp-offer-answer.md) and
> [msrp-tls.md](msrp-tls.md) for the TLS-specific review. The transport
> classes' own code is still unchanged; only which role is chosen, and by
> what code path, changed.
>
> **Task W111 update**: `SipCall` gained `sendMsrpMessage()`/
> `sendMsrpFile()` wrappers forwarding to its own `MsrpSession` (used by the
> new Client Messaging View — see
> [client-messaging-workspace.md](client-messaging-workspace.md)). The
> transport classes themselves are unchanged; this only exposes
> capabilities `MsrpSession` already had.

## Threading

Both transports are built entirely on Qt's own async socket classes
(`QTcpSocket`/`QSslSocket`/`QTcpServer`), which run on the Qt event loop.
Every signal (`connected`, `bytesReceived`, `errorOccurred`, `disconnected`,
`tlsHandshakeCompleted`) already fires on the Qt/UI thread — unlike pjsua2
callbacks (which fire on a PJSIP worker thread and require
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)` elsewhere in this
codebase), no marshaling is needed here. `connectActive()`/
`listenPassive()`/`sendBytes()` never call `waitForConnected()`/
`waitForReadyRead()`/any blocking Qt Network API — every result arrives
asynchronously via a signal.

## TCP (`MsrpTcpTransport`)

- **Active mode** (`connectActive`): `QTcpSocket::connectToHost()`, with a
  `QTimer`-driven connect timeout (`msrpConnectionTimeoutMs`) that calls
  `abort()` and emits `errorOccurred` if the socket hasn't reached
  `ConnectedState` in time.
- **Passive mode** (`listenPassive`): `QTcpServer::listen()` on
  `bindAddress:port` (port `0` ⇒ OS-assigned ephemeral port, the default
  under `msrpPortMode = automatic`). The first inbound connection is
  adopted and the listener is immediately closed (`server->close()`) — a
  single MSRP session's passive side accepts exactly one peer. An accept
  timeout (`acceptTimeoutMs`) closes the listener and reports an error if
  no peer ever connects.
- **Backpressure/buffer limits**: `QTcpSocket::setReadBufferSize(8 MiB)` on
  every socket bounds how much unread data Qt will buffer if the
  application is slow to consume it.
- **Bind vs. advertised host**: the transport only ever binds to
  `msrpLocalBindAddress` (or "any" if empty); the separate
  `msrpAdvertisedHost` setting (used only when building the outbound
  `a=path`/offer, in `MsrpSdpNegotiator`/the MSRP page) is never assumed to
  equal the bind address — no `0.0.0.0`/`localhost` is ever advertised as a
  reachable peer address.
- **Session association**: an inbound connection is associated with a
  session purely through which `MsrpSession`/`MsrpTcpTransport` instance
  is listening on that specific port — never by inspecting the peer's
  IP/port after the fact. (RFC 4975's own connection-reuse-by-path
  matching for a shared listening transport across many sessions is not
  implemented at this foundation stage — each `MsrpSession` currently owns
  its own transport instance/port.)

## TLS (`MsrpTlsTransport`)

- **Active mode**: `QSslSocket::connectToHostEncrypted(host, port, host)`
  — the third argument is also used for SNI and for the hostname the
  certificate is checked against.
- **Passive mode**: `QTcpServer` accepts a plain socket, which is then
  adopted into a `QSslSocket` via `setSocketDescriptor()` +
  `startServerEncryption()`. **Known limitation**: server-side TLS
  requires a local certificate/private key to be configured on the socket
  before `startServerEncryption()` can succeed in a real deployment; this
  foundation task provides the mechanism but no certificate-provisioning
  UI — see [msrp-security.md](msrp-security.md) and
  [msrp-testing.md](msrp-testing.md) for how the local test harness works
  around this (or is documented as not covering server-TLS end-to-end).
- **Verification**: `msrpTlsVerifyPeer` defaults `true`
  (`QSslSocket::setPeerVerifyMode(VerifyPeer)`); `sslErrors` are always
  surfaced via the `tlsErrors` signal and logged as an `MsrpDiagnosticsEvent`
  error. Only when `msrpTlsVerifyPeer` is explicitly `false` does the
  transport call `ignoreSslErrors()` — a config-gated bypass, never a
  silent default, and intended only for a local/self-signed test
  environment (see msrp-security.md).
- **CA configuration**: `msrpTlsCaPath`, if set, loads additional trusted
  CA certificates from a PEM file via `QSslConfiguration::setCaCertificates`.

## Connection timeouts

`msrpConnectionTimeoutMs` (active connect / passive accept) and
`msrpIdleTimeoutSeconds` (session inactivity — enforced by `MsrpSession`/
`MsrpChunkAssembler::purgeStale`, not by the transport itself) are both
configurable, never hardcoded.
