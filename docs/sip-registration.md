# SIP Registration

**Status:** IMPLEMENTED (Task 12; state machine Task 13; retry/backoff Task 14; expiry refresh Task 15; real registration requires PJSIP)

## Overview

`SipManager` registers and unregisters the active `SipProfile` through one owned `SipAccount`. All state transitions are enforced by `RegistrationStateMachine`. The implementation preserves the dependency-free stub backend and keeps all pjsua2 types behind `HAVE_PJSIP` guards.

## Registration States

There are five explicit states:

| State                | Meaning |
|---|---|
| `Unregistered`       | No active registration; register action is allowed |
| `Registering`        | REGISTER request in flight; actions rejected |
| `Registered`         | Active registration confirmed; unregister is allowed |
| `Unregistering`      | REGISTER with Expires:0 in flight; actions rejected |
| `RegistrationFailed` | Last attempt failed; retry/register is allowed |

Profile editing (Edit/Delete buttons) is disabled while the state is `Registering` or `Unregistering`.

## State Transition Table

| From               | To                    | Trigger |
|---|---|---|
| Unregistered       | Registering           | `registerActiveProfile()` — preflight passed |
| Unregistered       | RegistrationFailed    | `registerActiveProfile()` — preflight failed (no profile/credential) |
| Registering        | Registered            | `onRegState` — active registration confirmed |
| Registering        | RegistrationFailed    | `onRegState` error, or watchdog timeout (30 s default) |
| Registering        | Unregistering         | `unregisterActiveProfile()` called while Registering (cancel) |
| Registered         | Unregistering         | `unregisterActiveProfile()` |
| Registered         | RegistrationFailed    | `onRegState` — re-REGISTER refresh rejected by server |
| Unregistering      | Unregistered          | `onRegState` — inactive registration confirmed |
| Unregistering      | RegistrationFailed    | `onRegState` error, or watchdog timeout (30 s default) |
| RegistrationFailed | Registering           | `registerActiveProfile()` retry — preflight passed |
| RegistrationFailed | RegistrationFailed    | `registerActiveProfile()` retry — preflight failed again |
| RegistrationFailed | Unregistered          | `unregisterActiveProfile()` — cleanup after failure |

All other transitions (e.g. `Registered → Registering`, `Unregistered → Unregistered`) are rejected.
`reset()` unconditionally transitions to `Unregistered` from any state (used during shutdown).

## Retry / Backoff (Task 14)

On `RegistrationFailed`, `SipManager` checks `RegistrationRetryPolicy::isRetryable(statusCode)`:
- **Retryable:** `0` (timeout/no response), `408`, `5xx`
- **Not retryable:** `401`, `403`, `404`, `423`, and other 4xx

When retryable and `m_retryAttempt < maxAttempts`, a `QTimer` is started with:
```
delay = initialDelayMs × multiplier^(attempt−1), capped at maxDelayMs
```
Default: 5 attempts, 2 s initial, ×2, 60 s cap.

`unregisterActiveProfile()` and `shutdown()` cancel the retry timer.

## Registration Expiry & Auto Re-REGISTER (Task 15)

On a successful `Registered` callback, `SipManager::scheduleRefresh(expirySeconds)` is called. The refresh timer fires at:
```
min(expiry × 0.80,  expiry − 30 s)
```
with a floor of `expiry / 2` when expiry is very short, and a fallback default of 300 s when the server provides no expiry.

When the timer fires (`onRefreshTimerFired`):
- If a live `SipAccount` exists: `refreshRegistration()` is called which sends a new REGISTER on the same account object (`setRegistration(true)` in pjsua2). The state machine stays `Registered` during the refresh.
- If no account: fall through to `scheduleRetryIfEligible(0)`.

**Refresh success** (server returns 200 OK): retry counter resets, new refresh timer scheduled.  
**Refresh failure**: state transitions to `RegistrationFailed` (`Registered → RegistrationFailed` is now a valid SM transition); existing retry/backoff logic takes over.

`unregisterActiveProfile()` and `shutdown()` cancel the refresh timer.

`registrationStatusText()` returns `"Registered (refreshing...)"` while a refresh is in flight.

## Rejected Operations

| Attempted operation          | Current state      | Result |
|---|---|---|
| `registerActiveProfile()`    | Registering        | Rejected, returns false, WARN logged |
| `registerActiveProfile()`    | Unregistering      | Rejected, returns false, WARN logged |
| `registerActiveProfile()`    | Registered (different profile) | Rejected — must unregister first |
| `unregisterActiveProfile()`  | Unregistered       | No-op, returns true |
| `unregisterActiveProfile()`  | Unregistering      | No-op, returns true |

## Public API

```cpp
bool SipManager::registerActiveProfile();
bool SipManager::unregisterActiveProfile();

RegistrationState SipManager::registrationState() const;
QString           SipManager::registrationStatusText() const;
int               SipManager::registrationStatusCode() const;
QString           SipManager::registeredProfileId() const;
RegistrationStateMachine &SipManager::stateMachine();  // for test injection
```

`registrationStateChanged(state, statusText, statusCode)` is emitted on every state change. `SipAccount` marshals pjsua2 callbacks to the Qt event thread with a queued `QMetaObject::invokeMethod` call.

## Registration Flow

```text
Sidebar Register button
  -> SipManager::registerActiveProfile()
     -> check state (reject if Registering / Unregistering / Registered-other-profile)
     -> SipProfileManager::activeProfile()
     -> CredentialStore::loadPassword(profileId, authUsername)
     -> create/locate UDP, TCP, or TLS PJSIP transport (HAVE_PJSIP only)
     -> RegistrationStateMachine::tryTransition(Registering, ...)
     -> SipAccount::startRegistration(profile, password, transportId)
        -> pj::Account::create(AccountConfig, true)
        -> REGISTER and onRegState callback
     -> sidebar + status bar + diagnostics
```

## Watchdog Timeout

`RegistrationStateMachine` starts a 30-second single-shot timer whenever it enters `Registering` or `Unregistering`. If the timer fires before the state changes:

- Stuck in `Registering` → forced to `RegistrationFailed`
- Stuck in `Unregistering` → forced to `Unregistered`

`SipManager::onStateMachineTimedOut()` receives `transitionTimedOut` and calls `destroyAccount()`. The timeout interval can be overridden via `stateMachine().setTimeoutMs(ms)`.

## Credential Safety

- Passwords are loaded only through `CredentialStore` at registration time.
- Passwords are not members of `SipManager`, `SipAccount`, or `SipProfile`.
- Passwords are not persisted in application/profile settings.
- Passwords are never passed as state machine `reason` strings.
- Registration diagnostics contain profile IDs, status text, and status codes, never passwords.
- Regression tests use a known sentinel secret and verify it does not appear in emitted diagnostic messages.

## Diagnostics

Every state transition is logged by `RegistrationStateMachine`:

```
Registration SM: <OldState> → <NewState>; reason="<reason>"; status=<code>
```

Rejected transitions are logged at WARN:

```
Registration SM: rejected <OldState> → <Attempted>; <reason>
```

Timeouts:

```
Registration SM: timeout in state <State> after <N> ms
Registration SM: <State> → <Fallback> (timeout forced)
```

The SIP category also records register started, register success, register failure with status/code, unregister started, and unregister success from `SipManager`.

## Backend Behavior

### ENABLE_PJSIP=ON and PJSIP found

- `SipManager` creates the profile's UDP, TCP, or TLS transport if needed.
- `SipAccount` creates a real pjsua2 account with registrar, proxy, transport, identity, and digest credential configuration.
- pjsua2 sends REGISTER or unregister requests.
- `onRegState` maps successful active registration to `Registered`, successful inactive registration to `Unregistered`, and local/SIP errors to `RegistrationFailed`.
- SIP status/reason details are forwarded through Qt signals and diagnostics.

### ENABLE_PJSIP=OFF or PJSIP unavailable

- Endpoint initialization remains successful using `Stub SIP backend`.
- A registration attempt validates the active profile and credential, emits `Registering`, then safely emits `RegistrationFailed` with a local status message.
- Unregister safely returns to `Unregistered`.
- Stub mode never reports a false successful registration.

## GUI

The profile sidebar Register/Unregister button follows the state machine:

| State              | Button text        | Button enabled |
|---|---|---|
| Unregistered       | Register           | Yes (if profile selected) |
| Registering        | Register           | No |
| Registered         | Unregister         | Yes |
| Unregistering      | Unregister         | No |
| RegistrationFailed | Retry registration | Yes (if profile selected) |

Profile Edit and Delete buttons are disabled during `Registering` and `Unregistering`.

The global status bar shows the current state with color coding:
- Green (#50c878): Registered
- Amber (#e0b850): Registering / Unregistering
- Red (#e05050): Unregistered / RegistrationFailed

## Tests

`test_registration_state_machine` (27 tests) covers:
- All 11 valid transitions pass
- 6 invalid transitions are rejected with `transitionRejected` signal and no state change
- `stateChanged` signal not emitted on rejected transitions
- Timeout from `Registering` → `RegistrationFailed` (50 ms injected)
- Timeout from `Unregistering` → `Unregistered` (50 ms injected)
- `reset()` from all four non-Unregistered states
- State name stability for all five states
- Password-as-reason plumbing guard

`test_sip_manager` (14 tests) additionally covers:
- `registerActiveProfile()` rejected while Registering
- `registerActiveProfile()` rejected while Registered (different profile)
- `unregisterActiveProfile()` no-op while Unregistered
- `Unregistering` state is distinct from `Registering`
- Diagnostic password non-disclosure end-to-end

## Known Limitations (Task 13)

- No automatic registration on startup or profile selection.
- No refresh scheduling before registration expiry.
- No retry/backoff on transient failures.
- Network-change recovery not implemented.
- One account is supported at a time; multi-account is deferred.
- Stub mode intentionally cannot register successfully.
- Real registrar interoperability, TLS certificates, NAT behavior, and failure codes require PJSIP-enabled integration testing.
