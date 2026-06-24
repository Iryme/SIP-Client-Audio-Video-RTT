# PJSIP Audio Call Validation

Task 22 validates the real PJSIP path with `ENABLE_PJSIP=ON`. This is a validation and bug-fix pass only: video, RTT, ETSI behavior, call history, and SIP ladder improvements are out of scope.

## Live Validation Results

### Task 22C — Registration (2026-06-24, PASS)

Live REGISTER → 401 challenge → 200 OK against Kamailio validated via `live_registration_probe`. Re-REGISTER refresh fired and rescheduled. UNREGISTER with Expires:0 returned 200 OK. State machine reached Unregistered cleanly.

### Task 22D — Audio Call (2026-06-24, PASS)

Live outbound INVITE → peer answered → call reached Active. Audio codec: PCMU @ 8 kHz. PJSIP media callback fired, RTP audio bridge connected. RX and TX packets exchanged with 0% packet loss. BYE sent on hangup, call reached Idle. UNREGISTER returned 200 OK.

Outbound proxy (`SIP_LIVE_OUTBOUND_PROXY=sip:<server>;transport=udp`) required to prevent `PJ_ERESOLVE` when target AoR contains a hostname.

### Task 23A — GUI Live Audio Call (2026-06-24)

Validated the full audio call flow through the real GUI application (`build-pjsip-real\Debug\SIPClient.exe`).

**Issue found:** CallPanel had no dialing UI (no URI input, no Call button). Outbound calls could not be initiated from the GUI.

**Fix:** Added a minimal dial row to `CallPanel` — a `QLineEdit` for the SIP URI and a "Call" button, visible only in Idle state. The Call button is enabled only when registered. Both the button click and the Enter key trigger `SipManager::makeCall()`. Row hides when a call is in progress.

**GUI validation flow:**
1. Launch `build-pjsip-real\Debug\SIPClient.exe`.
2. Add profile via "+ Profile": username `1001`, domain `sensor-x.local`, registrar `10.2.0.180`, outbound proxy `sip:10.2.0.180;transport=udp`.
3. Click Register → status turns green (Registered).
4. In dial row: enter `sip:1002@sensor-x.local`, click Call.
5. Peer (MicroSIP/Linphone as `1002`) rings → answer.
6. Call state reaches Active (green). Level meters move.
7. Click Hangup → Idle.
8. Click Unregister → Unregistered. No crash. No PJSIP warning.

### Task 22E — Account Deletion Race Fix (2026-06-24)

**Issue:** PJSIP logged `Warning: deleting account 0 while call 0 is still active (forced)` when unregister immediately followed hangup.

**Root cause:** `SipManager::onActiveCallStateChanged` used `deleteLater()` to defer `SipCall` destruction, which was correct for Qt signal-stack safety but meant the `pj::Call` slot inside `SipCall` outlived the `pj::Account`. When unregister completed and `destroyAccount()` ran, the `pj::Account` was deleted while PJSIP still tracked call 0 as occupying a slot.

**Fix:** `SipCall::releasePjsipCall()` synchronously deletes `m_impl->pjCall` (and calls `pj::Call::hangup` if still active as a safety guard) before `deleteLater()` is scheduled. This frees the PJSIP call slot immediately while the Qt wrapper object's deferred cleanup proceeds normally. `SipCall::~SipCall()` checks `m_impl->pjCall != nullptr` before acting, so there is no double-delete.

## Local Status

Task 22B installed and validated a real local PJSIP backend on 2026-06-23. The local install prefix is:

```text
F:\Project\Iryme\SIP-Client-Audio-Video-RTT\.deps\pjsip-msvc-install
```

The local PJSIP build used pjproject commit `469aa47`, Debug configuration, WMME/null audio, and WASAPI disabled because the installed Windows SDK does not provide `phoneaudioclient.h`.

The real-backend project build was configured with:

```powershell
cmake -S . -B build-pjsip-real `
  -DENABLE_PJSIP=ON `
  -DPJSIP_DIR=F:\Project\Iryme\SIP-Client-Audio-Video-RTT\.deps\pjsip-msvc-install `
  -DBUILD_TESTS=ON `
  -DCMAKE_PREFIX_PATH=F:\Programs\Qt\6.11.1\msvc2022_64
```

The app and tests compile with `HAVE_PJSIP`; `test_sip_manager` asserts `SipManager::backendName() == "PJSIP/pjsua2"`. Live Kamailio registration, INVITE, RTP, and BYE were validated in Tasks 22C–22E (see results above).

### Live Validation Tool

When you want a focused registration-only check, build the probe with `BUILD_LIVE_VALIDATION_TOOLS=ON`:

`BUILD_LIVE_VALIDATION_TOOLS` is `OFF` by default.

```cmd
cmake -S . -B build-pjsip-real ^
  -DENABLE_PJSIP=ON ^
  -DPJSIP_DIR=F:\Project\Iryme\SIP-Client-Audio-Video-RTT\.deps\pjsip-msvc-install ^
  -DBUILD_TESTS=ON ^
  -DBUILD_LIVE_VALIDATION_TOOLS=ON ^
  -DCMAKE_PREFIX_PATH=F:\Programs\Qt\6.11.1\msvc2022_64

cmake --build build-pjsip-real --config Debug --target live_registration_probe
```

The probe reads `SIP_LIVE_SERVER`, `SIP_LIVE_PORT`, `SIP_LIVE_DOMAIN`, `SIP_LIVE_USERNAME`, and `SIP_LIVE_PASSWORD` from the environment for the current process only.

The application currently proves digest auth by reaching `Registered`, but it does not yet emit an explicit app-level log entry for the intermediate `401 Unauthorized`/`407 Proxy Authentication Required` challenge. The PJSIP backend handles the challenge correctly, but the log evidence is still indirect until a dedicated signaling trace is added.

### Live Audio Call Tool

Use `live_audio_call_probe` for a live outbound audio call through Kamailio to a peer that is already registered and ready to answer.

The probe reads these environment variables:

- `SIP_LIVE_SERVER`
- `SIP_LIVE_PORT`
- `SIP_LIVE_DOMAIN`
- `SIP_LIVE_USERNAME`
- `SIP_LIVE_PASSWORD`
- `SIP_LIVE_TARGET`
- `SIP_LIVE_OUTBOUND_PROXY` (optional)

`SIP_LIVE_TARGET` may be a full SIP URI such as `sip:1002@sensor-x.local` or a bare username such as `1002`; the probe normalizes bare usernames to the configured domain when no outbound proxy is set.

If `SIP_LIVE_OUTBOUND_PROXY` is set, the probe keeps `SIP_LIVE_TARGET` as the Request-URI and sends the INVITE through the proxy instead of rewriting the target to an IP address.

Build both live tools with:

```cmd
cmake -S . -B build-pjsip-real ^
  -DENABLE_PJSIP=ON ^
  -DPJSIP_DIR=F:\Project\Iryme\SIP-Client-Audio-Video-RTT\.deps\pjsip-msvc-install ^
  -DBUILD_TESTS=ON ^
  -DBUILD_LIVE_VALIDATION_TOOLS=ON ^
  -DCMAKE_PREFIX_PATH=F:\Programs\Qt\6.11.1\msvc2022_64

cmake --build build-pjsip-real --config Debug --target live_audio_call_probe
```

Validation flow:

1. Register the probe account.
2. Place an INVITE to `SIP_LIVE_TARGET`.
3. Confirm the call reaches `Active`.
4. Confirm audio media becomes active from the PJSIP callback.
5. Speak for a short window while the call remains connected.
6. Hang up.
7. Unregister.

Known limitation: the app logs still do not explicitly capture the intermediate `401 Unauthorized` challenge as a named app event. Digest authentication is validated by the successful REGISTER and call setup path.

### Why INVITE Can Fail With `PJ_ERESOLVE`

If you call `sip:paul@sensor-x.local` without an outbound proxy, PJSIP tries to resolve `sensor-x.local` directly for the INVITE next hop. In this setup that can fail with `PJ_ERESOLVE`, even though REGISTER succeeds against `10.2.0.180`.

Quick workaround:

```text
SIP_LIVE_TARGET=sip:paul@10.2.0.180
```

Correct fix:

```text
SIP_LIVE_OUTBOUND_PROXY=sip:10.2.0.180;transport=udp
SIP_LIVE_TARGET=sip:paul@sensor-x.local
```

With the proxy set, the Request-URI remains the peer AoR and the proxy decides the next hop.

## Test Matrix

Use three SIP accounts on the same Kamailio realm:

| Role | Example | Client |
| --- | --- | --- |
| App under test | `sip:1001@example.test` | SIP Client Audio Video RTT |
| Outgoing-call peer | `sip:1002@example.test` | Linphone or MicroSIP |
| Incoming-call peer | `sip:1003@example.test` | Linphone or MicroSIP |

Replace `example.test`, usernames, passwords, and proxy addresses with the actual Kamailio values. Use UDP first unless the server requires TCP or TLS.

## Build And Launch

1. Open a Visual Studio x64 developer prompt.
2. Configure with a real PJSIP install:

```cmd
cmake -S . -B build-pjsip -DENABLE_PJSIP=ON -DPJSIP_DIR=C:\path\to\pjsip\install -DBUILD_TESTS=ON -DCMAKE_PREFIX_PATH=F:\Programs\Qt\6.11.1\msvc2022_64
```

3. Build:

```cmd
cmake --build build-pjsip --config Debug
```

4. Run:

```cmd
build-pjsip\Debug\SIPClient.exe
```

5. Confirm logs contain:

```text
SIP backend initialized (PJSIP/pjsua2)
```

## Registration Validation

1. Create or select the app SIP profile:
   - SIP URI: `sip:1001@example.test`
   - Registrar: `example.test` or `sip:example.test`
   - Auth username: `1001`
   - Password: account password in the credential store
   - Transport: UDP
   - Outbound proxy: only if Kamailio requires one
2. Click Register.
3. Confirm Kamailio returns the expected digest challenge and successful registration.
4. Confirm application logs contain:

```text
PJSIP REGISTER create account
PJSIP registration callback
Registration SM: Registering
Registration SM: Registering -> Registered
```

Pass criteria:

- The first REGISTER receives `401 Unauthorized` or `407 Proxy Authentication Required`.
- PJSIP resends REGISTER with digest authorization.
- Final registration response is `200 OK`.
- The app state becomes `Registered`.

## Linphone Steps

### Linphone Account Setup

1. Install Linphone Desktop.
2. Add SIP account:
   - Username: `1002`
   - Password: password for `1002`
   - Domain: `example.test`
   - Transport: UDP
   - Disable video for this validation if Linphone prompts for call mode.
3. Confirm Linphone registers successfully.

### App Outgoing INVITE To Linphone

1. In the app, enter `sip:1002@example.test`.
2. Click Call.
3. Answer in Linphone.
4. Speak both directions for at least 20 seconds.
5. Hang up from the app.

Pass criteria:

- Logs contain `PJSIP INVITE outbound`.
- Linphone rings and answers.
- Logs contain `PJSIP media state callback` and `PJSIP RTP audio bridge connected`.
- Audio is heard in both directions.
- Logs contain `PJSIP BYE/hangup requested` and `Call disconnected`.
- Linphone returns to idle.

### Linphone Incoming INVITE To App

1. From Linphone, call `sip:1001@example.test`.
2. Confirm the app shows an incoming call.
3. Click Answer in the app.
4. Speak both directions for at least 20 seconds.
5. Hang up from Linphone, then repeat and hang up from the app.

Pass criteria:

- Logs contain `PJSIP incoming INVITE` and `PJSIP INVITE inbound bound`.
- App answer sends `200 OK`; logs contain `PJSIP INVITE answer 200 OK`.
- Audio is heard in both directions.
- Remote BYE tears down the app call cleanly through idle.

## MicroSIP Steps

### MicroSIP Account Setup

1. Install MicroSIP.
2. Add account:
   - SIP Server: `example.test`
   - SIP Proxy: leave empty unless Kamailio requires a proxy
   - Username: `1002` or `1003`
   - Domain: `example.test`
   - Login: same as username unless Kamailio uses a different auth user
   - Password: account password
   - Transport: UDP
   - SRTP: disabled
3. Confirm MicroSIP shows online/registered.

### App Outgoing INVITE To MicroSIP

1. In the app, call `sip:1002@example.test`.
2. Answer in MicroSIP.
3. Verify two-way audio for at least 20 seconds.
4. Hang up from each side in separate runs.

Pass criteria:

- Outgoing INVITE reaches MicroSIP.
- RTP audio flows both directions.
- App and MicroSIP both return to idle after BYE.

### MicroSIP Incoming INVITE To App

1. From MicroSIP, call `sip:1001@example.test`.
2. Answer in the app.
3. Verify two-way audio for at least 20 seconds.
4. Hang up from each side in separate runs.

Pass criteria:

- App receives incoming call and can answer.
- Logs show media active and RTP bridge connected.
- BYE teardown is clean with no crash and no stuck call state.

## Evidence To Capture

For each pass, save:

- App log from launch through unregister/shutdown.
- Kamailio access/syslog lines for REGISTER, INVITE, ACK, RTP relay if applicable, and BYE.
- Linphone or MicroSIP call log.
- Screenshot of the app registered state and active call state.

## Failure Notes

If registration fails before digest retry, verify username, auth username, realm, registrar URI, and transport. If INVITE succeeds but audio is one-way, check Windows audio permissions, selected microphone/speaker, local firewall UDP media ports, NAT/contact rewriting, and whether Kamailio is using RTP relay.
