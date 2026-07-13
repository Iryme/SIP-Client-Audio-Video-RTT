# MSRP Relay Live Call Integration (Task W108)

## Config (opt-in, disabled by default)

`AppSettings` (`src/core/AppSettings.h`), all under `msrp/relay/*`, all
empty/off by default — nothing here is ever hardcoded to a provider, host,
port, or account:

`msrpRelayMode` (`"disabled"`/`"automatic"`/`"required"`, default
`"disabled"`), `msrpRelayHost`, `msrpRelayPort`, `msrpRelayUseTls`,
`msrpRelayUsername`, `msrpRelayCredentialProfileId` (a `CredentialStore`
lookup key — the password itself is never stored in `AppSettings`),
`msrpRelayTlsVerifyPeer`, `msrpRelayTlsCaPath`, `msrpRelayConnectTimeoutMs`,
`msrpRelayAuthTimeoutMs`, `msrpRelayRefreshMarginSeconds`,
`msrpRelayMaxRetries`, `msrpRelayPreparationTimeoutMs`. There is currently
**no UI** for these settings (same "config/INI only" status as W107's relay
component) — they must be set directly in the INI file or by test code.

## Outbound calls

`SipManager::dispatchMakeCall()` (`src/sip/SipManager.cpp`) is the shared
tail end of `makeCall(uri)` and `makeCall(uri, CallMediaOptions)` (called
after `prepareOutgoingCall()` has already created `m_activeCall`):

1. Builds an `MsrpRelayConfig` from `AppSettings` (`buildMsrpRelayConfigFromSettings()`).
2. **If relay mode is `Disabled` or the config is not `isUsable()`** (no
   host/username set): calls `m_activeCall->makeCallWithOptions()`
   immediately — the identical synchronous call it always was. No
   controller is created. **This is the path every existing call takes
   today**, since the default is `Disabled`.
3. Otherwise: looks up the relay password from `CredentialStore` (only if
   `credentialProfileId` is set), starts a (lazily-created, reused)
   `MsrpCallPreparationController`, and defers `makeCallWithOptions()` to
   its `ready` signal. `makeCall()` still returns `true` immediately
   (meaning "accepted", as it always has — actual dialing progress is
   observed via `callStateChanged`, unchanged); the visible effect is a
   short delay (bounded by `msrpRelayPreparationTimeoutMs`) before the
   `OutgoingInit` state transition and the INVITE actually go out.
4. `controller::failed` (Required mode, no allocation) resets the freshly
   created `SipCall` back to `Idle` via `SipCall::reset()` with a
   descriptive reason — the call never reaches PJSIP at all.

**Emergency calls bypass this entirely** — `SipManager::makeEmergencyCall()`
still calls `m_activeCall->makeCallWithOptions()` directly, with an explicit
comment recording that this is deliberate (an emergency call must never be
delayed by a relay allocation).

`SipManager::destroyActiveCall()` calls `cancelMsrpCallPreparation()`
unconditionally before tearing down `m_activeCall` — covers hangup, a new
call replacing this one, shutdown, and profile switch with a single hook
(see [msrp-relay-call-preparation.md](msrp-relay-call-preparation.md),
"Cancellation safety").

### SDP injection and transport adoption

See [msrp-relay-async-sdp-architecture.md](msrp-relay-async-sdp-architecture.md)
for the full mechanism. Concretely, in `SipCall.cpp`:

- `onCallSdpCreated`'s offerer branch checks
  `m_impl->preparedMsrpOffer.ready && mode == Relay` before doing anything
  else MSRP-related; if true, it uses `preparedMsrpOffer.advertisedUri`
  (the real allocated Use-Path) instead of calling
  `startMsrpPassiveListener()`.
- Immediately after `pj::Call::makeCall()` returns (dialog exists),
  `SipCall::makeCallWithOptions()` calls
  `preparedMsrpOffer.relayClient->takeTransport()` and
  `msrpSession->adoptExternalTransport(std::move(transport), MsrpRole::ActiveConnector)`.

### Verified (automated, no live relay/SIP server needed)

`tests/test_msrp_call_preparation.cpp` (6 cases) against a scripted local
RFC 4976 relay double (`FakeMsrpRelay`, real TCP, real MSRP AUTH frames):
direct fallback when relay disabled, "MSRP not wanted" resolves to `None`,
successful relay allocation produces a `Ready` offer with a real Use-Path
and an allocated `MsrpRelayClient`, `Required`-mode failure emits `failed()`,
`Automatic`-mode failure falls back to `Direct`, and cancellation
mid-allocation suppresses any late `ready()`/`failed()`.

The full W090–W108 regression suite (75/75 `ctest` targets) passes
unmodified — see the W108 report for the exact baseline/final counts.

### NOT verified (no live relay/SIP-Server-RTT access in this session)

- A live relay allocation actually landing in a real outbound INVITE's wire
  bytes (`a=path` containing the relay's Use-Path) was **not observed** —
  this session had no relay credentials available (see
  [msrp-relay-live-validation.md](msrp-relay-live-validation.md)).
- The relay setup role (`MsrpSetup::ActPass`, unchanged from the direct-MSRP
  offer) has not been verified as correct for relay-assisted MSRP against a
  real relay — RFC 4976 relay topology means neither peer connects directly
  to the other, which may have setup-role implications this implementation
  has not investigated. **Documented as an open question, not resolved.**
- `MsrpRelayClient::takeTransport()`/`adoptExternalTransport()` interaction
  is unit-tested for ownership (`test_msrp_session_relay_transport.cpp`,
  Task W107) but has never been exercised end-to-end through an actual
  `SipCall` against a live relay.

## Inbound calls — NOT IMPLEMENTED

Answering a peer-initiated `m=message` offer that requires (or would
benefit from) a relay allocation is **not implemented** in W108. The
existing inbound answer branch in `onCallSdpCreated`
(`!isOfferer && !prm.remSdp.wholeSdp.empty()`) still only knows about
direct MSRP (`startMsrpPassiveListener()`/`startMsrpActiveConnect()`, Task
W102).

**Why this was not attempted**: unlike the outbound path — where
`SipManager::dispatchMakeCall()` has a clean synchronous entry point to
defer *before* `pj::Call::makeCall()` is ever invoked — the inbound path's
equivalent "defer before responding" point is inside pjsua2's own
auto-answer/SDP-negotiation machinery (`onCallState`/the 200 OK response
construction), and this codebase has not audited whether pjsua2 exposes a
safe way to hold a provisional response open while an async relay
allocation completes, versus what the existing auto-answer/manual-answer
flow already assumes about `onCallSdpCreated` completing synchronously
within a single PJSIP transaction callback. Attempting this without being
able to verify it against a real inbound INVITE (no live relay peer
available this session) risked regressing the existing, working inbound
call answer path for audio/video/RTT — assessed as too high a blast radius
for an unverified change.

**Status: BLOCKED** (architectural investigation needed, not merely
unimplemented code) — recommended as the first item for a follow-up task.
Inbound calls with a peer that only offers a relay path (and no usable
direct fallback) will currently be answered with `m=message` rejected
(port 0) — the same documented behavior as any other MSRP offer this
client cannot service, never a dropped call.
