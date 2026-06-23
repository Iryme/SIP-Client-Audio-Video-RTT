# PJSIP Audio Call Validation

Task 22 validates the real PJSIP path with `ENABLE_PJSIP=ON`. This is a validation and bug-fix pass only: video, RTT, ETSI behavior, call history, and SIP ladder improvements are out of scope.

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

The app and tests compile with `HAVE_PJSIP`; `test_sip_manager` asserts `SipManager::backendName() == "PJSIP/pjsua2"`. Live Kamailio registration, INVITE, RTP, and BYE validation are still the next step.

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
