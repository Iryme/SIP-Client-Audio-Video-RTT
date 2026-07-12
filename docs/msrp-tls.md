# MSRP TLS (Task W102 review)

This is a review/audit document for Task W102 Phase 7 — it does not
introduce new TLS code. See [msrp-transport.md](msrp-transport.md) for
`MsrpTlsTransport` itself and [msrp-security.md](msrp-security.md) for the
broader security posture (both from Task W100, unchanged by W102).

## What W102 verified (by reading the existing implementation, not by
## claiming new coverage)

- **Certificate/CA configuration**: `msrpTlsCaPath` (config, never
  hardcoded) is loaded via `QSslConfiguration::setCaCertificates` —
  confirmed still wired unchanged in `MsrpTlsTransport`.
- **Peer verification default**: `msrpTlsVerifyPeer` defaults `true`
  (`QSslSocket::setPeerVerifyMode(VerifyPeer)`); verification is only ever
  bypassed (`ignoreSslErrors()`) when that setting is explicitly `false` —
  confirmed there is no code path that silently disables verification.
- **Hostname verification**: the active-connect path
  (`connectToHostEncrypted(host, port, host)`) uses the same `host` value
  for both the TCP target and the certificate-name check (SNI + hostname
  verification) — sourced from the negotiated `a=path` host, never
  hardcoded.
- **Timeouts**: `msrpConnectionTimeoutMs` is configurable and applies to
  both TCP and TLS active connects.
- **Error surfacing**: `sslErrors` is always relayed into
  `MsrpDiagnosticsEvent` (visible in the MSRP page's diagnostics table and
  the JSON export) — never silently swallowed, and the message text is the
  Qt-provided SSL error description, not a raw certificate dump (no secret
  material logged).

## What W102 explicitly did NOT change or newly validate

- **Server-side TLS** (a passive-listening MSRP session that must present
  its own certificate) still has the same "mechanism exists, no
  certificate-provisioning UI" gap documented in W100/msrp-transport.md —
  W102's peer-initiated-offer answer path (msrp-offer-answer.md) can
  negotiate `msrps://` and choose the passive role, but if this client is
  ever the passive/TLS-server side of a real handshake, it still needs a
  local certificate/key configured before `startServerEncryption()` can
  succeed — this was not built or tested in W102.
- **No live TLS handshake against a real peer** was performed in this
  session (no SIP-Server-RTT/Blink/AG Projects TLS-capable MSRP endpoint
  was available) — see
  [msrp-live-interoperability.md](msrp-live-interoperability.md) for the
  explicit `BLOCKED`/`NOT RUN` breakdown. The active-connect + hostname
  verification code path above was read and confirmed correct by
  inspection, not exercised end-to-end.
- **Self-signed test-mode** (an explicit, non-default opt-in to skip
  verification for local/lab testing) is the pre-existing
  `msrpTlsVerifyPeer=false` config flag from W100 — W102 did not add a
  separate "test mode" toggle; using the same flag for that purpose is
  already possible and already gated (never a silent default).

## Conclusion

No TLS security regression or improvement was made in W102: the existing
W100 TLS implementation was audited against this task's explicit
requirements (certificate configurability, hostname verification, CA
trust, no implicit disabled verification, timeouts, redacted error
surfacing) and found to already satisfy them for the active-connect side.
Server-side (passive/listening) TLS provisioning remains a documented gap.
