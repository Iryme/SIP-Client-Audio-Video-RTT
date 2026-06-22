# SIP Registration

**Status:** IMPLEMENTED (Task 12; real registration requires PJSIP)

## Overview

`SipManager` registers and unregisters the active `SipProfile` through one owned `SipAccount`. The implementation preserves the dependency-free stub backend and keeps all pjsua2 types behind `HAVE_PJSIP` guards.

## Public API

```cpp
bool SipManager::registerActiveProfile();
bool SipManager::unregisterActiveProfile();

RegistrationState SipManager::registrationState() const;
QString SipManager::registrationStatusText() const;
int SipManager::registrationStatusCode() const;
QString SipManager::registeredProfileId() const;
```

Registration states are `Unregistered`, `Registering`, `Registered`, and `RegistrationFailed`.

`registrationStateChanged(state, statusText, statusCode)` is emitted for state changes. `SipAccount` marshals pjsua2 registration callbacks to the Qt event thread with a queued `QMetaObject::invokeMethod` call.

## Registration Flow

```text
Sidebar Register button
  -> SipManager::registerActiveProfile()
     -> SipProfileManager::activeProfile()
     -> CredentialStore::loadPassword(profileId, authUsername)
     -> create/locate UDP, TCP, or TLS PJSIP transport
     -> SipAccount::startRegistration(profile, password, transportId)
        -> pj::Account::create(AccountConfig, true)
        -> REGISTER and onRegState callback
     -> sidebar + status bar + diagnostics
```

The authentication username is `authUsername` when configured, otherwise `sipUsername`. The registrar and proxy are normalized to SIP URIs. The selected profile transport is created once per endpoint and assigned to the account.

Unregister uses `pj::Account::setRegistration(false)`. Account destruction calls pjsua2 `Account::shutdown()` before deleting the derived account, and application shutdown destroys the account before the endpoint.

## Credential Safety

- Passwords are loaded only through `CredentialStore` at registration time.
- Passwords are not members of `SipManager`, `SipAccount`, or `SipProfile`.
- Passwords are not persisted in application/profile settings.
- Registration diagnostics contain profile IDs, status text, and status codes, never passwords.
- The stub regression suite uses a known sentinel secret and verifies that it does not occur in emitted diagnostic messages.

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

The profile sidebar includes a Register/Unregister button. Its account card shows registration state with red, amber, or green status styling and exposes status detail as a tooltip.

The global status bar mirrors registration state and provides the status text/code as a tooltip. During unregister, the four-state model uses `Registering` with status text `Unregistering`.

## Diagnostics

The SIP category records register started, register success, register failure with status/code, unregister started, and unregister success. No password or authorization header is logged.

## Tests

`test_sip_manager` covers endpoint lifecycle, stub behavior, stable state names, missing active-profile failure, safe stub register/unregister transitions, and diagnostic password non-disclosure.

At Task 12 completion, a fresh Windows Visual Studio 2026 Debug stub build succeeded and all five CTest suites passed. Real PJSIP registration could not be integration-tested because PJSIP was not installed; guarded API usage was checked against the current official pjproject headers.

## Known Limitations

- No automatic registration on startup/profile selection.
- No refresh scheduling before registration expiry.
- No retry/backoff, registration timeout, or network-change recovery state machine.
- One account is supported at a time.
- Switching profiles does not provide a staged wait for the old unregister response.
- Stub mode intentionally cannot register.
- Real registrar interoperability, TLS certificates, NAT behavior, and failure codes require PJSIP-enabled integration testing.
