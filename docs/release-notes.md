# Release Notes

See [versioning-and-rollout.md](versioning-and-rollout.md) for the versioning policy, rollout gate, and branch model.

---

## v1.3.0 — Settings Media Configuration (Microphone / Speaker)

**Status:** pre-release validation
**Branch:** `release/v1.3.0`
**Version bump type:** MINOR
**Reason:** New Settings UI surface and a new (additive) volume-control API layered on existing PJSIP audio device plumbing; no SDP wire-level or registration logic change.

### Relevant commits (chronological)

| Commit | Description |
|--------|-------------|
| `4350f03` | Add settings media configuration for audio devices |

### Changes

**Settings → Media tab**
- New tab in Settings (next to Video), implemented in `MediaSettingsPanel`.
- Grouped Microphone / Speaker sections: device dropdown, live level meter, volume slider, and (for the speaker) a Test Speaker button.

**Microphone / speaker device selection**
- Dropdowns populated from real enumerated devices (`MediaDeviceManager::listMicrophones()/listSpeakers()`), with a "Default (system)" entry.
- Selecting a device calls `AudioMediaManager::setMicrophone()/setSpeaker()` — the same API already used by CallPanel — so both surfaces stay consistent.
- Refresh Devices button triggers `MediaDeviceManager::refreshDevices()` (async Qt Multimedia re-enumeration) and preserves the current selection if the device is still present.
- Missing/disappeared device: falls back to the default via the existing `MediaDeviceSelectionModel` resolution logic, with a warning logged and surfaced in the UI (disabled combo + tooltip when no devices exist at all).

**Real volume control via PJSIP**
- New `AudioMediaManager::setMicrophoneVolume()/setSpeakerVolume()` and `SipCall::setMicVolume()/setSpeakerVolume()`, previously entirely absent from the codebase.
- Microphone gain: `pj::AudDevManager::getCaptureDevMedia().adjustTxLevel(percent / 100.0f)`.
- Speaker gain: `pj::AudDevManager::getPlaybackDevMedia().adjustRxLevel(percent / 100.0f)`.
- Applied immediately to the active call's PJSIP audio media when connected; otherwise stored and applied as soon as audio media connects on the next/current call.

**Persistence in QSettings**
- New keys `media/volume/microphone` and `media/volume/speaker` (default 100 = unity gain) alongside the existing `media/device/microphone`/`media/device/speaker` keys, loaded at startup via `AppSettings`.

**CallPanel sync**
- CallPanel's microphone/speaker volume sliders — previously always disabled with a "not available in current backend" tooltip — are now enabled and wired to the same `AudioMediaManager` API.
- Changing volume or device in Settings updates CallPanel live (and vice versa) via `AudioMediaManager::microphoneVolumeChanged`/`speakerVolumeChanged` signals.

**Test Speaker**
- Real playback (not simulated): generates a short 440 Hz sine tone and plays it via `QAudioSink` on the currently selected output device.

**Mute restores user volume**
- Fixed `SipCall::setMuted()`, which previously hardcoded the unmute level to `1.0f` (ignoring any user-configured microphone volume). Unmuting now restores the persisted `micVolume` gain instead of resetting it to full.

### Manual validation checklist

- [ ] Settings → Media tab shows Microphone and Speaker sections with device dropdown, live meter, and volume slider.
- [ ] Changing microphone/speaker in Settings updates the active call's audio device without crashing.
- [ ] Changing volume in Settings updates CallPanel's sliders live, and vice versa.
- [ ] Test Speaker plays an audible tone through the selected output device.
- [ ] Unplugging/removing the selected device falls back to default with a warning shown in the UI and logged.
- [ ] No microphone/speaker present: dropdown disabled with a clear tooltip; Test Speaker disabled when no speaker is present.
- [ ] Mute then unmute during an active call restores the previously set microphone volume (not full volume).
- [ ] Build: full CMake build exits 0 (`ENABLE_PJSIP=ON`).
- [ ] Tests: all 27 ctest tests pass.

### Constraints respected

- Registration logic unchanged.
- SIP server configuration unchanged.
- No fake/simulated meter values — levels come from `AudioMediaManager::inputLevelChanged`/`outputLevelChanged`.
- No push performed until build + full ctest pass.

---

## v0.4.0 — Dashboard UI, Media Consent Popups, RTT Flow, Camera Control

**Status:** pre-rollout / pending validation  
**Branch:** `feature/web-ui-tabs-sip-ladder-details`  
**Version bump type:** MINOR  
**Reason:** New UI panels and new signal flows; no SDP wire-level or registration logic change.

### Relevant commits (chronological)

| Commit | Description |
|--------|-------------|
| `1cc8730` | Fix camera preview restart, unhold bottom metrics and client cards |
| `1c66a3d` | Add real RTP stats to status bar |
| `462231a` | Fix safe call teardown after hangup |
| `668dc77` | Fix incoming call controls, video consent, camera toggle, RTP stats, incoming popup |
| `ec868af` | Fix camera off during video call and video request button state |
| `2d4aa6d` | Fix video accept state and camera toggle when no active video stream |
| `d24dc20` | Add dashboard quick register and media request UI |

### Changes

**Dashboard Quick SIP Actions (Cerință A)**
- Profile dropdown: lists all configured SIP profiles; changing selection sets the active profile without auto-registering.
- Register/Unregister button: four visual states tracking `RegistrationState` (Unregistered → Register green; Registering → disabled yellow; Registered → Unregister red; Unregistering → disabled yellow).
- Live status label: shows `<state> | sip:user@domain | registrar` with color-coded state.

**Incoming Media Request Popups (Cerință B)**
- New `MediaRequestDialog` — non-blocking, floats over any tab (`Qt::Tool | WindowStaysOnTopHint | FramelessWindowHint`).
- Video request: shows caller URI, "Accept Video" / "Ignore". Accept sends re-INVITE; Ignore dismisses without SIP action.
- RTT request: shows caller URI, "Accept RTT" / "Ignore". Accept calls `requestCallRtt(true)`.
- Auto-dismisses on: call idle/failed, media channel active, peer withdraw.
- Does NOT auto-accept. User action required in all cases.

**Dynamic Text Protocol Button (Cerință C)**
- `textProtocolLabel()` reads `SipProfile::enableLmpe` / `enableRtt` to select label ("RTT" or "LMPE").
- RTT is the only functionally implemented text protocol. LMPE label appears only if the active profile sets `enableLmpe=true && enableRtt=false`.
- Button state machine: Request RTT → (incoming) Accept RTT → RTT Active (disabled).

**RTT Consent Flow**
- New `rttRequested()` signal: `onCallRxReinvite` detects `m=text` in re-INVITE, sets `textCount=0` (auto-decline), emits signal. User must explicitly accept.
- `rttRequestNotified` flag mirrors `videoRequestNotified` to prevent duplicate signals.

**Camera Off During Active Video Call (Cerință D)**
- Required log messages added to `SipManager::setCallVideoMuted()`:
  - "Camera Off requested during active call"
  - "Stopping local video transmit" / "Local video transmit stopped"
  - "Camera On requested during active call"
  - "Starting local video transmit" / "Local video transmit started"

**Bugfixes included in this MINOR**
- Qt assert: `Qt::UniqueConnection` with lambda connections in `DashboardPage` — removed (unsupported combination in Qt6 debug builds).
- Camera freeze on call-end: guard added to skip PJSIP stream op when no video stream is active (`PJ_ENOTFOUND` prevention).
- Hold button state sync after resume confirmation timeout.
- Video accept state and camera toggle when no active video stream.

### Manual validation checklist

- [ ] Dashboard tab: profile combo lists all profiles; changing profile does not auto-register.
- [ ] Dashboard tab: Register → "Registering…" → "Unregister" when registered.
- [ ] Dashboard tab: Unregister → "Unregistering…" → "Register" when unregistered.
- [ ] During active call: remote sends re-INVITE with video → popup appears → Accept Video → video activates → popup closes.
- [ ] During active call: remote sends re-INVITE with RTT → popup appears → Accept RTT → RTT activates → popup closes.
- [ ] Ignore on popup: no SIP action taken, popup closes.
- [ ] Camera Off during active video call: logs appear in Diagnostics, remote stream freezes (camera stopped), no crash on call end.
- [ ] RTT button label: default profile → "Request RTT"; LMPE-only profile → "Request LMPE".
- [ ] No auto-accept of video or RTT in any scenario.
- [ ] Build: `codex_build_vsdev` exits 0.
- [ ] Tests: all 12 ctest tests pass.

### Known issues / deferred

- Camera Off during active video call freezes remote view instead of gracefully removing the local transmit stream. The PJSIP `PJSUA_CALL_VID_STRM_STOP_TRANSMIT` op may send a freeze frame rather than a stream removal re-INVITE. Full fix deferred — see open investigation.

### Constraints respected

- Registration logic unchanged.
- SIP server configuration unchanged.
- No invented jitter/loss/RTT values.
- No auto-accept of video or RTT.
- No push performed.

---

## v0.3.x — SIP Ladder, Video Panel, Emergency, Audio/RTP (historical)

Covers commits prior to `1cc8730` on `feature/project-skeleton`.  
Status: integrated — see `feature/project-skeleton` branch history.

---

## MAJOR — Future roadmap

The following items are explicitly deferred to a future **MAJOR** release. See [versioning-and-rollout.md](versioning-and-rollout.md) for rationale.

| Feature | Target |
|---------|--------|
| LMPE real — ETSI TS 103 698 | vMAJOR (TBD) |
| Zoiper / Linphone messaging compatibility matrix | vMAJOR (TBD) |
| Full protocol negotiation: RTT / LMPE / SIP MESSAGE / MSRP | vMAJOR (TBD) |
