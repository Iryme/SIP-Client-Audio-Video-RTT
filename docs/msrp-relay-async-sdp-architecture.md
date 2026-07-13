# MSRP Relay: Why SDP Construction and Relay Allocation Must Be Split (Task W108)

## The constraint

`SipCall::Impl::PjCall::onCallSdpCreated()` (see
[msrp-live-sdp-integration.md](msrp-live-sdp-integration.md)) is a pjsua2
callback invoked **synchronously**, on the same call stack as
`pj::Call::makeCall()` (outbound) or PJSIP's own answer-SDP construction
(inbound). It must:

- never block (no `QEventLoop::exec()`, no `waitForConnected()`/
  `waitForReadyRead()`, no `sleep`);
- never perform network I/O of its own;
- return quickly, because pjsip's internal locks/transaction timers are
  running while it executes.

RFC 4976 relay allocation is the opposite: a real TCP/TLS connect, a
401 challenge round-trip, and an authenticated AUTH round-trip — on the
order of hundreds of milliseconds to several seconds, never instantaneous.
Performing it inside `onCallSdpCreated` was never viable.

## The split (Task W108)

1. **`MsrpCallPreparationController`** (`src/msrp/MsrpCallPreparationController.h/.cpp`)
   runs entirely *before* `SipCall::makeCallWithOptions()` is ever called. It
   owns a `MsrpRelayClient` for the duration of allocation and produces a
   **`PreparedMsrpOffer`** (`src/msrp/MsrpCallPreparation.h`) — a small,
   already-resolved data struct.
2. **`SipCall::setPreparedMsrpOffer()`** stores that struct on `SipCall::Impl`
   before the call is created.
3. **`onCallSdpCreated`** (`src/sip/SipCall.cpp`, the outbound/offerer branch)
   only ever *reads* `m_impl->preparedMsrpOffer`. It performs exactly one of:
   - relay mode: copies the already-allocated Use-Path into the SDP
     (`MsrpSipMediaInjector::injectMessageMedia`) and creates the (not yet
     transport-bound) `MsrpSession` object — no I/O;
   - direct mode / no offer prepared: exactly the original W101-era
     synchronous `startMsrpPassiveListener()` path (a local `QTcpServer::listen()`
     call, which is itself non-blocking and was already part of the
     pre-W108 callback — unchanged).
4. Once `pj::Call::makeCall()` returns (dialog created), `SipCall::makeCallWithOptions()`
   adopts the relay's already-connected transport into the just-created
   `MsrpSession` via `MsrpRelayClient::takeTransport()` /
   `MsrpSession::adoptExternalTransport()` — this is likewise non-blocking
   (moving an already-open socket, not opening a new one).

## What this buys

- `onCallSdpCreated` is byte-for-byte unchanged in the default case (relay
  disabled, the shipped default) — see `AppSettings::msrpRelayMode()`
  defaulting to `"disabled"`.
- When relay mode is enabled, the callback still performs no I/O: it either
  reads an offer that is already fully resolved, or falls back to the
  pre-existing direct-listener path.
- Relay allocation failures are handled entirely *before* the callback
  fires (see [msrp-relay-call-preparation.md](msrp-relay-call-preparation.md)),
  so `onCallSdpCreated` never has to decide what to do about a failed
  allocation — by the time it runs, that decision has already been made.

## What this does NOT do

This split only covers the **outbound offer** path. Answering a
peer-initiated relay-relevant `m=message` offer (inbound calls) is not
implemented — see [msrp-relay-live-call-integration.md](msrp-relay-live-call-integration.md),
"Inbound calls — NOT IMPLEMENTED".
