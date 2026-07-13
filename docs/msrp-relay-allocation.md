# MSRP Relay Allocation (Use-Path) — Task W107

## `MsrpRelayAllocation`

`src/msrp/MsrpRelayAllocation.h` — the only way an instance is ever created
is `MsrpRelayClient::parseUsePath()` parsing a real `200 OK` response's
`Use-Path`/`Expires` headers. No code path constructs one from a guessed or
locally-invented URI.

```cpp
struct MsrpRelayAllocation {
    bool valid;
    QList<MsrpUri> usePath;   // first entry = our newly-reserved relay URI
    QDateTime allocatedAt;
    QDateTime expiresAt;
    QString allocationId;        // internal correlation only, never on the wire
    QString relayConnectionId;   // internal correlation only, never on the wire
};
```

- `usePath` is parsed with the existing `MsrpPath::parsePath()` — the same
  RFC 4975 URI-list parser already used for ordinary `a=path` handling, so
  there is no new/duplicated URI-parsing logic for relay paths.
- `expiresAt` defaults to `now + 600s` if the relay's `200 OK` omits
  `Expires` (RFC 4976's documented default), otherwise it's `now +
  Expires-header-value`.
- `allocationId`/`relayConnectionId` are `QUuid`-generated, used only to
  correlate diagnostics/export rows and (future) session mapping — never
  transmitted, never derived from anything sent on the wire.

## Validation performed before an allocation is accepted

`MsrpRelayClient::parseUsePath()` rejects (does not populate `*out`, and the
caller then treats it as a protocol-category failure, retried like any
other failure) when:

- `Use-Path` is missing or empty on an otherwise-`2xx` response.
- The header does not parse to at least one syntactically valid MSRP URI
  (`MsrpUri::ok`).

Deliberately **not** validated further at this layer: DNS-resolvability of
the allocated host, or that the allocated port is actually reachable — RFC
4976 offers no such guarantee at allocation time (the relay is telling the
client what it will accept connections on, not proving it is reachable from
this arbitrary client's network position), and inventing such a check would
be indistinguishable from a fabricated pass/fail signal.

## Dialog ↔ allocation ↔ session mapping

`MsrpRelayClient::setCorrelation(sipCallIdRedacted, mediaIndex)` exists so a
future caller (per call) can tag every diagnostics event this client
produces with which SIP dialog/media index it belongs to — the same
`sipCallIdRedacted`/`mediaIndex` fields already used by
`MsrpSessionInfo`/`MsrpDiagnosticsEvent` for direct-MSRP peer association
(Task W101/W102). One `MsrpRelayClient` instance is scoped to exactly one
control connection/allocation; two simultaneous calls using the same relay
account would each own their own `MsrpRelayClient` (this task does not
create that per-call ownership — see
[msrp-relay-authentication.md](msrp-relay-authentication.md)'s "What is
deliberately not wired" — but the correlation fields exist so W108's
integration doesn't need another schema change).

## Expiry, refresh, and what happens when refresh fails

- `scheduleRefresh()` arms a one-shot timer for
  `max(1, secondsUntilExpiry - refreshMarginSeconds)`.
- A refresh cycle reuses the *same* control connection and repeats the full
  `AUTH`/challenge/response exchange (the live relay test showed the relay
  issuing a fresh nonce each time rather than accepting nonce-count reuse
  across cycles — see [msrp-relay-sip2sip-validation.md](msrp-relay-sip2sip-validation.md)
  — so this implementation never assumes nonce reuse works and always
  re-challenges).
- If the refresh's `AUTH` cycle exhausts `maxRetries`,
  `MsrpRelayAllocation::valid` is cleared and `allocationLost` fires with a
  reason string — the caller (once W108 wires a real caller) is expected to
  apply `MessagingTransportPolicy` at that point exactly as it already does
  for a direct-MSRP failure, not invent new fallback logic.
- `MsrpRelayAllocation::isExpired()`/`needsRefresh()` are pure, side-effect-free
  helpers any caller can use to defensively check an allocation's freshness
  before using it, independent of whether the refresh timer has fired yet.
