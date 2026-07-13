# MSRP Relay Call Lifecycle (Task W108)

## Fallback policy

| Relay mode | Allocation fails before call creation | Result |
|---|---|---|
| `Disabled` | n/a — never attempted | Direct MSRP (or nothing, if MSRP itself is off) — unchanged pre-W108 behavior |
| `Automatic` | Yes | `MsrpCallPreparationController::finishFailed()` calls `resolveDirect(Direct)` — the call proceeds with direct MSRP; audio/video/RTT/messaging are never blocked by a relay-only failure |
| `Required` | Yes | Controller emits `failed(reason)`; `SipManager` resets the freshly-created `SipCall` back to `Idle` (`SipCall::reset()`) — **no INVITE is ever sent** |

There is no scenario in which two transports (relay + direct, or relay +
SIP MESSAGE) are used simultaneously for the same message — the resolved
`PreparedMsrpOffer::mode` is a single decision made once, before the call
exists.

## Cancellation / races (Task W108 Phase 6)

Covered by `MsrpCallPreparationController`'s generation-token mechanism
(see [msrp-relay-call-preparation.md](msrp-relay-call-preparation.md),
"Cancellation safety") plus `SipManager::destroyActiveCall()` calling
`cancelMsrpCallPreparation()` unconditionally. Enumerated against the
spec's race list:

- **User cancels during DNS-SRV/TLS-connect/post-AUTH allocation**: covered
  generically — `cancel()` at any point before `Ready`/`Failed`/`Cancelled`
  bumps the generation and tears down the in-flight `MsrpRelayClient`.
  (DNS-SRV discovery itself is out of scope for this controller — see
  [msrp-relay-live-call-integration.md](msrp-relay-live-call-integration.md).)
- **Allocation succeeds after cancel**: the generation guard drops the
  late `allocationReady` signal — `ready()` never fires, no call is
  created.
- **App shuts down during allocation**: `SipManager::shutdown()` already
  calls `destroyActiveCall()` (unchanged), which now also cancels
  preparation.
- **Two calls started rapidly**: `SipManager::prepareOutgoingCall()`
  rejects a second call while one is already active/non-Idle (unchanged
  guard) — a preparation in flight does not yet have an `m_activeCall` in
  a non-Idle state (state transitions to `OutgoingInit` only once
  `makeCallWithOptions()` actually runs), so this specific edge —
  starting a *second* call while the *first* is still in
  `ResolvingPolicy`/`AllocatingRelay` — is **not fully guarded** by the
  existing "already active" check and is a documented gap for a follow-up
  task. In practice `m_activeCall` already exists (non-null) from the
  first `prepareOutgoingCall()`, and `destroyActiveCall()` is called again
  before the second attempt creates a new one, which cancels the first
  preparation — but this has not been exercised by an automated test.
- **Allocation callback after controller destruction**: guarded by Qt's
  own `QPointer`/parent-child lifetime — every deferred lambda captures a
  `QPointer<MsrpCallPreparationController> self` and checks `!self` first.
- **Timeout and success in the same window**: the timeout lambda also
  checks the generation and the current state (`Ready`/`Failed`/`Cancelled`
  already reached ⇒ no-op) before acting, so a success that lands in the
  same event-loop turn as a timeout firing cannot double-resolve.

## Refresh during an active call — NOT IMPLEMENTED

`MsrpRelayClient`'s refresh cycle (Task W107) continues to run for the
lifetime of the `PreparedMsrpOffer::relayClient` object (kept alive on
`SipCall::Impl`). When it fires:

- if the transport was never adopted (`takeTransport()` not yet called, or
  call setup failed before adoption): refresh behaves exactly as in W107's
  own tests — reconnect-and-reauthenticate, `allocationRefreshed`/`allocationFailed`.
- **if the transport *was* adopted into the call's `MsrpSession`** (the
  normal case for an established relay call): `MsrpRelayClient::m_transport`
  is null (ownership moved — see
  [msrp-relay-call-preparation.md](msrp-relay-call-preparation.md),
  "Ownership"), so the refresh always takes the "reconnect from scratch"
  branch and produces a **new** allocation with a **new** Use-Path via
  `allocationRefreshed()`.

**W108 does not react to `allocationRefreshed()` fired mid-call.** No code
currently listens for it once the offer has been consumed by
`SipCall::makeCallWithOptions()`; the new allocation is silently unused,
and the *old* Use-Path — no longer valid at the relay — remains what the
established `MsrpSession`'s adopted transport was using. In practice this
means: **a relay-assisted call's MSRP session may stop working (silently,
from the relay's point of view) once the original allocation's `Expires`
elapses**, unless `msrpRelayRefreshMarginSeconds`/the allocation's expiry
is longer than any realistic call duration. This is a real, documented
limitation, not a hidden gap — closing it requires wiring
`allocationRefreshed()` to a re-INVITE/UPDATE with the new `a=path`
(Phase 12 of the original task spec), which was not implemented this
session.

## Reallocation / re-INVITE — NOT IMPLEMENTED

Directly follows from the above: since nothing reacts to
`allocationRefreshed()`, no re-INVITE/UPDATE is ever sent to carry a
changed relay path to the peer. `SipCall` has existing re-INVITE machinery
for video/RTT (`requestVideo`/`requestRtt`) that a future task could
extend for this purpose, but W108 does not touch it.

## Cleanup

`SipCall`'s existing teardown (`reset()`, destructor) is unmodified by
W108 — `preparedMsrpOffer.relayClient`'s `shared_ptr` (deleteLater
deleter) is released along with the rest of `SipCall::Impl`, which safely
schedules the `MsrpRelayClient`'s destruction without a synchronous
in-signal-handler delete (see
[msrp-relay-call-preparation.md](msrp-relay-call-preparation.md),
"Ownership").
