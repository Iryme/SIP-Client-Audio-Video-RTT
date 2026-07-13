# MSRP Relay Security — Task W107

Mirrors [msrp-security.md](msrp-security.md)'s discipline for the new relay
code path.

## Never hardcoded

`MsrpRelayConfig` (`src/msrp/MsrpRelayConfig.h`) has no default host, port,
username, or password — `mode` defaults to `Disabled` and every other field
defaults to empty/zero. The relay host/port/username must come from a
profile/config/UI/environment the caller supplies; `MsrpRelayClient` never
resolves DNS or picks a default relay itself.

## Credential handling

`MsrpRelayClient::configure(config, password)` takes the password as a
plain argument, held only in the `MsrpRelayClient` instance's memory for
the lifetime of the AUTH handshake (`m_password`) — the relay client itself
never touches `CredentialStore`. The intended flow (for whichever caller
eventually wires this per-call, per W108) is the same pattern already used
by `SipAccount`/`XcapClient`: load from `CredentialStore::loadPassword()`
at the point of use, pass it in, never persist it anywhere else. No test in
this task, and no live-validation script written for this task, ever
writes a real password to a git-tracked file — the live probe
(`tests/live_msrp_relay_probe.cpp`) reads `MSRP_RELAY_LIVE_PASSWORD` from
the environment only.

## What must never be logged (and how that's enforced structurally, not by convention)

`MsrpRelayDiagnosticsEvent` (`src/msrp/MsrpRelayDiagnosticsEvent.h`) has no
field that could hold a nonce, digest response, or credential — there is no
free-form "raw request" field to accidentally populate with one, unlike a
naive design that might log the full `Authorization`/`WWW-Authenticate`
header text. Specifically:

- `algorithm` (e.g. `"MD5"`) and `qopUsed` (bool) are logged — never the
  `nonce`, `cnonce`, `response`, or `opaque` values themselves.
- `allocatedPathRedacted` is host:port only (via `redactHostPort()`),
  stripping the session-id/token portion of the allocated URI.
- `sipCallIdRedacted` is passed in already-redacted by the caller (same
  convention as `MsrpDiagnosticsEvent::sessionKey` truncation elsewhere).
- `allocationId`/`relayConnectionId` are internal `QUuid`s, never the
  relay's own session-id.

`test_windows_trace_json_export.cpp`'s
`exportMsrpRelayEventsFieldsRedacted` test asserts this structurally: it
checks the exported JSON object's key set never contains `"nonce"`,
`"password"`, `"digest"`, or `"credentials"` — a schema-level guarantee, not
just "we didn't add that field this time."

## TLS

`MsrpRelayClient` reuses `MsrpTlsTransport` unmodified — `tlsVerifyPeer`
defaults to `true` in `MsrpRelayConfig` (never silently disabled), and a
custom CA path is only honored when explicitly configured, matching
`MsrpSession`'s own existing TLS policy (see msrp-security.md). No new TLS
code was written for this task.

## Experimental status

The relay diagnostics panel added to `MsrpPage` is labeled "(RFC 4976,
Experimental)" in its group-box title, and — because `MsrpRelayConfig::mode`
defaults to `Disabled` and nothing in the live call path (`SipCall`/
`SipManager`) reads it yet (see
[msrp-relay-authentication.md](msrp-relay-authentication.md)) — relay
support has **zero effect on any real call** in this build regardless of
what a user does in that panel. This satisfies the task's "dezactivat
implicit până la validare live completă" requirement by construction, not
by a separate feature flag that could drift out of sync.
