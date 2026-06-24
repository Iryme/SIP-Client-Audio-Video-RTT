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

### Task 23D — GUI Audio Validation Complete (2026-06-24, PASS)

Full end-to-end GUI audio call confirmed working against the live Kamailio server at `10.2.0.180`.

**Confirmed working:**
- Register from GUI → Kamailio 200 OK, state turns green
- Outbound call from GUI dial row → INVITE routed via outbound proxy → peer answers → call reaches Active
- Audio heard in both directions
- Hangup from GUI → BYE sent cleanly, call reaches Idle
- Unregister from GUI → 200 OK, state returns to Unregistered
- No crash, no stuck state machine

**GUI validation flow (exact steps):**

1. Launch `build\SIPClient.exe` from an x64 MSVC environment with PJSIP enabled.
2. In the Accounts sidebar, click `+ Profile` and fill in:
   - Username: your SIP username (e.g. `alice`)
   - Domain: `sensor-x.local`
   - Registrar: `10.2.0.180`
   - Auth username: same as username (or the digest auth user if different)
   - Password: stored in Windows Credential Manager on first save
   - Transport: UDP
   - Outbound proxy: `sip:10.2.0.180` — **required** to prevent `PJ_ERESOLVE` on INVITE
3. Click Register → status label turns green (Registered).
4. In the dial row (bottom of the call panel), type the peer's SIP URI, e.g. `paul` (auto-normalized to `sip:paul@sensor-x.local`) or a full URI.
5. Click Call — INVITE is sent via the outbound proxy.
6. Peer answers on their SIP client → call state reaches Active, level meters animate.
7. Click Hangup → BYE sent, call state returns to Idle.
8. Click Unregister → state returns to Unregistered.

**Note — audio device selector:** wired to PJSIP `AudDevManager` in Task 24A. Changing devices during an active call still takes effect on the next call only.

### Task 24A — GUI Audio Device Selection Wired to PJSIP (2026-06-24, PASS)

The microphone and speaker combos in the Media panel now control PJSIP's `AudDevManager`. Device selection persists across app restarts and takes effect on the next call.

**What was implemented:**

- `PjsipAudioMapper` (new static helper) maps Qt display names to PJSIP device indices using case-insensitive exact/substring scoring against `adm.getDevInfo(i).name`. Returns -1 (PJSIP default) when no match is found.
- `AudioMediaManager` gained an `audioDeviceSelectionChanged()` signal emitted from `setMicrophone()` and `setSpeaker()`.
- `MediaPanel` forwards combo selections to `AudioMediaManager::setMicrophone/setSpeaker` via lambda connections.
- `SipManager::applyPersistedAudioDevices()` reads `MediaDeviceSelectionModel`, resolves display names to PJSIP indices via `PjsipAudioMapper::applyDevicesByName()`, and calls `adm.setCaptureDev/setPlaybackDev`. Called at PJSIP init and whenever `audioDeviceSelectionChanged` or `devicesChanged` fires.
- All PJSIP device enumeration is logged at init: index, name, input count, output count.

**Known limitation:** changing the device during an active call does not rewire the active RTP bridge. The new selection takes effect on the next call.

**Validation:**

1. Launch `build\SIPClient.exe` with PJSIP enabled.
2. Open Media panel — microphone and speaker combos populate from `QMediaDevices`.
3. Select a non-default microphone. Check log: `Applying audio device selection: mic="..." speaker="..."` and `PjsipAudioMapper: "..." → capture[N]`.
4. Close and reopen app — selection is restored from QSettings.
5. Register, place a call — call audio uses the selected capture device. Log confirms `PJSIP audio devices applied: capture=N playback=M`.
6. All 13 unit tests pass (ctest 13/13).

### Task 25A — PJSIP Video Path Validation (2026-06-24)

**Objective:** Determine whether the PJSIP build has video support, and validate the video media path as far as it can go without a video codec or DirectShow capture device.

#### PJSIP video build status

| Item | Status |
|------|--------|
| `PJMEDIA_HAS_VIDEO` | **1** (enabled via cmake interface compile definition on `Pj::pjmedia`) |
| `pjmedia-videodev.lib` | Present in `.deps\pjsip-msvc-install\bin\` |
| `PJMEDIA_VIDEO_DEV_HAS_DSHOW` | **0** — DirectShow capture backend not compiled |
| `PJMEDIA_VIDEO_DEV_HAS_AVI` | **1** — AVI file source compiled (file playback only) |
| Video capture devices | **0** at runtime — no Windows camera capture backend |
| Video codecs | **0** at runtime — VPX=OFF, OpenH264=OFF, FFMPEG=OFF |

#### What was implemented (Task 25A)

1. **`SipManager::logVideoSubsystemStatus()`** (new static function, called in `initialize()`):
   - Logs `PJMEDIA_HAS_VIDEO` status on startup
   - Enumerates all video devices via `pj::Endpoint::instance().vidDevManager().getDevInfo()`
   - Enumerates all video codecs via `pj::Endpoint::instance().videoCodecEnum2()`
   - Logs explicit warnings when no devices or codecs are found

2. **`SipAccount::startRegistration()`** — account video config:
   - `cfg.videoConfig.autoTransmitOutgoing = true` — PJSIP will attempt to include video in INVITE
   - `cfg.videoConfig.autoShowIncoming = true` — PJSIP will accept incoming video streams

3. **`SipCall::onCallMediaState()`** — enhanced video media logging:
   - Active: logs `pjsipCallId`, `mediaIndex`, `videoIncomingWindowId`, `videoCapDev`
   - Inactive: logs `pjsipCallId`, `mediaIndex`, `status` for non-active video streams

#### Expected startup log output

```
[Media] PJSIP video support: ENABLED (PJMEDIA_HAS_VIDEO=1)
[Media] PJSIP video devices (0 total):
[Media] WARN: PJSIP video: no capture/render devices (PJMEDIA_VIDEO_DEV_HAS_DSHOW=0; no DirectShow backend compiled)
[Media] PJSIP video codecs (0 total):
[Media] WARN: PJSIP video: no video codecs available (VPX=OFF, OpenH264=OFF, FFMPEG=OFF). INVITE will not include a video media line.
```

#### Why INVITE does not offer video

With `PJMEDIA_HAS_VIDEO=1` but zero video codecs registered, PJSIP does not add an `m=video` line to the outgoing INVITE SDP. This is correct behaviour — without a codec, there is nothing to negotiate. The existing audio media (PCMU/PCMA) is unaffected.

To validate video negotiation, the PJSIP build must be rebuilt with at least one video codec:

| Codec | Build flag | Notes |
|-------|-----------|-------|
| VP8/VP9 | `-DPJMEDIA_WITH_VPX=ON` after installing libvpx | Most common for WebRTC-compatible SIP |
| H.264 | `-DPJMEDIA_WITH_OPENH264=ON` after installing OpenH264 | Required for interop with iOS/Android |
| FFMPEG codecs | `-DPJMEDIA_WITH_FFMPEG=ON` | Broadest codec support |

Additionally, a Windows camera capture backend requires:
```
-DPJMEDIA_WITH_VIDEODEV_DSHOW=ON
```
This requires the Windows SDK `dshow.h` (part of the Windows Desktop SDK).

#### Audio regression test

Audio calls remain fully functional. The video config changes (`autoTransmitOutgoing=true`) have no effect on the SDP when there are no video codecs, so no audio regression is possible.

#### Test results

13/13 unit tests pass (ctest, 2026-06-24).

#### What remains for actual video

1. Rebuild PJSIP with `PJMEDIA_WITH_VIDEODEV_DSHOW=ON` and at least one video codec (e.g. `-DPJMEDIA_WITH_VPX=ON` after installing libvpx, or `-DPJMEDIA_WITH_FFMPEG=ON`).
2. The PJSIP endpoint video subsystem and account config are already wired. On next call with video codecs available, PJSIP will include `m=video` in the INVITE.
3. When `onCallMediaState` fires with `PJMEDIA_TYPE_VIDEO / PJSUA_CALL_MEDIA_ACTIVE`, the log will show `videoIncomingWindowId` — this is the native window handle that the remote video stream renders into.
4. Actual Qt widget rendering: embed the PJSIP render window (by win32 HWND) into `VideoPanel` using `QWindow::fromWinId()` + `QWidget::createWindowContainer()`.

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
