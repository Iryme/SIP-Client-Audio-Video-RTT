# MSRP Relay Call Preparation (Task W108)

## Purpose

Resolves, asynchronously and *before* a SIP call is created, whether an
outbound call's MSRP media will use direct MSRP or a relay allocation — see
[msrp-relay-async-sdp-architecture.md](msrp-relay-async-sdp-architecture.md)
for why this has to happen outside `onCallSdpCreated`.

## State machine

`MsrpCallPreparationController::State` (`src/msrp/MsrpCallPreparationController.h`):

```
Idle -> ResolvingPolicy -> AllocatingRelay -> Ready
                        -> PreparingDirectMsrp -> Ready
                        -> Failed   (Required mode, no allocation)
                        -> Expired -> (Failed | Ready via fallback)
      -> Cancelled  (from any non-terminal state)
```

- **ResolvingPolicy**: `start()` decides, from the caller-supplied
  `MsrpRelayConfig` and a `wantsMsrp` flag, whether relay is even in play.
  If MSRP isn't wanted, or relay mode is `Disabled`/config `!isUsable()`,
  resolution goes straight to a `Direct`/`None`-mode `Ready` — this is the
  exact pre-W108 behavior, just reached through one deferred event-loop
  turn (`QTimer::singleShot(0, ...)`) instead of a direct call, so every
  caller goes through the same asynchronous API regardless of outcome.
- **AllocatingRelay**: a `MsrpRelayClient` (Task W107) is created, configured,
  and started. `allocationReady`/`allocationFailed` drive the next
  transition.
- **Ready**: emits `ready(const PreparedMsrpOffer &offer)` exactly once.
  `offer.mode` is `None`/`Direct`/`Relay`; for `Relay`, `offer.advertisedUri`
  is the real allocated Use-Path and `offer.relayClient` is a
  `std::shared_ptr<MsrpRelayClient>` (deleteLater-deleter — see "Ownership"
  below) kept alive for the call.
- **Failed**: only reachable when `MsrpRelayConfig::mode == Required` and
  allocation did not succeed (including timeout) — the caller must not
  create a call with an invented/absent MSRP offer; see
  [msrp-relay-call-lifecycle.md](msrp-relay-call-lifecycle.md) for what
  `SipManager` does with `failed()`.
- **Automatic mode never reaches Failed**: an allocation failure or timeout
  under `Automatic` calls `resolveDirect(Direct)` — relay is optional, so
  audio/video/RTT/messaging are never blocked by a relay-only failure. This
  mirrors `MessagingTransportPolicy`'s existing fallback philosophy (Task
  W090).
- **Cancelled**: `cancel()` bumps an internal generation counter and tears
  down any in-flight `MsrpRelayClient`. See "Cancellation safety" below.

## Cancellation safety

Every relay-client signal connection captures the generation counter's
value *at connect time* (a local `const int generation`). The lambda checks
`generation != self->m_generation` before acting. `cancel()` and a fresh
`start()` both bump the generation, so:

- a `MsrpRelayClient::allocationReady`/`allocationFailed` signal that
  arrives after `cancel()` is silently dropped — it can never emit
  `ready()`/`failed()` for a preparation the caller has already abandoned;
- calling `start()` again while a previous attempt is still in flight
  invalidates that previous attempt the same way, rather than needing two
  separate code paths.

This is unit-tested directly:
`TestMsrpCallPreparation::cancelDuringAllocationSuppressesLateSignals`
(`tests/test_msrp_call_preparation.cpp`) starts a real allocation against a
scripted local relay, cancels immediately, then waits 500ms for the
real network round-trip to complete in the background and asserts neither
`ready()` nor `failed()` ever fired.

## Ownership

| Object | Created by | Owned by | Destroyed by |
|---|---|---|---|
| `MsrpRelayClient` (preparation phase) | `MsrpCallPreparationController::startRelayAllocation()` | `std::shared_ptr` with a `deleteLater()` deleter (`makeDeleteLaterSharedPtr`, `MsrpCallPreparation.h`) — held by the controller until `ready()`, then co-owned by the emitted `PreparedMsrpOffer` | last `shared_ptr` reference dropped → `deleteLater()` (never a synchronous `delete`, so it is safe to drop from inside one of the client's own signal handlers) |
| `PreparedMsrpOffer::relayClient` | `MsrpCallPreparationController::finishReady()` | `SipCall::Impl::preparedMsrpOffer` (set via `SipCall::setPreparedMsrpOffer()`), for the lifetime of the call | same `shared_ptr` mechanism; released when the call is destroyed |
| Underlying `MsrpTransport` (the relay TCP/TLS socket) | `MsrpRelayClient::openTransport()` during allocation | `MsrpRelayClient` until `takeTransport()` is called; then `MsrpSession` (via `adoptExternalTransport`) | whichever object currently owns it |

**Why the transport moves but the client doesn't get destroyed**: RFC 4976
multiplexes control traffic (AUTH refresh) and session traffic (SEND/
response/REPORT) over the *same* connection. `MsrpRelayClient::takeTransport()`
(`src/msrp/MsrpRelayClient.h/.cpp`) hands the already-connected,
already-authenticated `MsrpTransport` to `MsrpSession::adoptExternalTransport()`
(Task W107) and disconnects its own signal handlers from that transport —
but the `MsrpRelayClient` object itself is kept alive (inside
`PreparedMsrpOffer::relayClient`) so its refresh timer can still fire.

**Documented consequence**: after `takeTransport()`, `MsrpRelayClient::m_transport`
is null. A later refresh (`onRefreshTimerFired()`,
`src/msrp/MsrpRelayClient.cpp`) always takes the "connection dropped —
reconnect from scratch" branch, producing a **new** allocation via
`allocationRefreshed()` with a **new** Use-Path — this is not a bug, it is
the only correct behavior once the connection has been handed off; the
caller must react to `allocationRefreshed()` by calling `takeTransport()`
again and re-negotiating SDP (re-INVITE/UPDATE) with the new path. **This
reaction is not wired up in W108** — see
[msrp-relay-call-lifecycle.md](msrp-relay-call-lifecycle.md), "Refresh
during an active call — NOT IMPLEMENTED".

## Not part of this controller

- DNS SRV discovery is not performed here — `MsrpRelayConfig::relayHost`
  must already be a concrete host (matches the W107 config model: "nu
  rezolvă DNS singur").
- Credential lookup happens in the caller (`SipManager::dispatchMakeCall()`,
  via `CredentialStore`) before `start()` is called — the controller never
  touches `CredentialStore` itself.
