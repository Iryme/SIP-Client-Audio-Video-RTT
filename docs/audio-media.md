# Audio Media Integration

**Task 18** — Added in branch `feature/audio-media`.

## Overview

Audio media integration enables real RTP audio communication when PJSIP is available (`ENABLE_PJSIP=ON`). In stub mode (default, `ENABLE_PJSIP=OFF`) the call control state machine works fully; audio wiring is skipped.

## Architecture

```
MediaDeviceManager          AudioMediaManager
  (device enumeration)  ──▶ (application-layer coordinator)
                              │  attachCall() / detachCall()
                              │  setMuted() / isMuted()
                              │  inputLevel() / outputLevel()
                              │
                      ┌───────▼──────────┐
                      │    SipCall        │  (owns CallStateMachine)
                      │  #ifdef HAVE_PJSIP│
                      │  onCallMediaState │  ──▶ pj::AudDevManager
                      │  onLevelTimerFired│       capture ◀──▶ call audio ◀──▶ playback
                      └──────────────────┘
```

### `AudioMediaManager` (`src/media/AudioMediaManager.h/.cpp`)

Application-layer singleton. Qt-only (no `pjsua2.hpp`).

- `attachCall(SipCall*)` — wires to SipCall signals; called by SipManager on call creation.
- `detachCall()` — disconnects signals; called by SipManager on call end (Idle/Failed) or shutdown.
- `setMuted(bool)` — delegates to `SipCall::setMuted()`; in PJSIP mode this adjusts the capture device tx level.
- `setMicrophone(id)` / `setSpeaker(id)` — persists via `MediaDeviceSelectionModel`; logs diagnostic. Hot-swap during an active call requires PJSIP integer device-index mapping (scaffolded; takes effect on next call).
- `inputLevel()` / `outputLevel()` — forwarded from `SipCall::inputLevelChanged` / `outputLevelChanged` (0–100, ~10 Hz in PJSIP mode; 0 in stub mode).

### `SipCall` additions

New signals:
- `audioMediaConnected()` — stub: emitted when state reaches Active; PJSIP: emitted from `onCallMediaState` when bridge is up.
- `audioMediaDisconnected()` — emitted when bridge is torn down or call ends.
- `muteChanged(bool)`
- `inputLevelChanged(int)` / `outputLevelChanged(int)` — ~10 Hz while media is active.

New methods:
- `setMuted(bool)` — stub: flag + signal; PJSIP: `AudDevManager::getCaptureDevMedia().adjustTxLevel()`.
- `isMuted() const`

### PJSIP audio bridge (inside `#ifdef HAVE_PJSIP` in `SipCall.cpp`)

`SipCall::Impl::PjCall::onCallMediaState()` iterates `CallInfo.media`. When an audio stream reaches `PJSUA_CALL_MEDIA_ACTIVE`:

```
ep.audDevManager().getCaptureDevMedia() ──startTransmit──▶ callAudioMedia
callAudioMedia ──startTransmit──▶ ep.audDevManager().getPlaybackDevMedia()
```

On `PJSIP_INV_STATE_DISCONNECTED`, `stopTransmit` is called on both paths.

Level monitoring uses the pjsua C API:
```c
pjsua_conf_port_id slot = pjsua_call_get_conf_port(callId);
pjsua_conf_get_signal_level(slot, &txLevel, &rxLevel);
```

### `SipManager` additions

- `setCallMuted(bool)` / `isCallMuted()` — delegates to active `SipCall`.
- Forwarded signals: `audioMediaConnected`, `audioMediaDisconnected`, `callMuteChanged`, `callInputLevelChanged`, `callOutputLevelChanged`.
- `makeCall()` and `onAccountIncomingCall()` both call `AudioMediaManager::instance().attachCall(m_activeCall)`.
- `onActiveCallStateChanged()` (Idle/Failed path) and `destroyActiveCall()` both call `AudioMediaManager::instance().detachCall()` — idempotent.

### `CallPanel` additions

- **Microphone selector** (`m_micSelector`) — `QComboBox` populated from `MediaDeviceManager`, visible only during an active call, changes call `AudioMediaManager::setMicrophone()`.
- **Speaker selector** (`m_spkSelector`) — same, calls `AudioMediaManager::setSpeaker()`.
- **Input level meter** (`m_inputMeter`) — green `QProgressBar` (0–100), labelled "Mic:", wired to `AudioMediaManager::inputLevelChanged`.
- **Output level meter** (`m_outputMeter`) — blue `QProgressBar` (0–100), labelled "Spk:", wired to `AudioMediaManager::outputLevelChanged`.
- **Mute button** (`m_btnMute`, was a placeholder) — now wired: `toggled` → `AudioMediaManager::setMuted()`; `AudioMediaManager::mutedChanged` → `onMuteChanged()` → button text and checked state sync with `QSignalBlocker`.

## Diagnostics

All audio lifecycle events are logged at `LogCategory::Media`:

| Event | Message |
|---|---|
| Audio media connected | `Audio media connected` |
| Audio media disconnected | `Audio media disconnected` |
| Audio media disconnected on detach | `Audio media disconnected (call detached)` |
| Mute enabled | `Mute enabled` |
| Mute disabled | `Mute disabled` |
| Device selected | `Microphone selected: <displayName>` |
| Device changed during call | `Microphone device changed during active call: <name> (PJSIP hot-swap scaffolded...)` |
| Device not found | `setMicrophone: device id '<id>' not found` |

In `SipCall.cpp` at `LogCategory::Sip`:

| Event | Message |
|---|---|
| Mute ON | `Mute ON: call id=<id>` |
| Mute OFF | `Mute OFF: call id=<id>` |
| Audio connected | `Audio media connected: id=<id>` |
| Audio disconnected | `Audio media disconnected: id=<id>` |

## Tests (`tests/test_audio_media.cpp`)

6 Qt Test cases (`QTEST_GUILESS_MAIN`). All tests bypass SipManager and drive `SipCall` directly via `AudioMediaManager::attachCall()`.

| Test | Description |
|---|---|
| `connectMediaOnActive` | `attachCall` + drive to Active → `mediaConnected` emitted, `isMediaActive()` true |
| `disconnectMediaOnHangup` | hangup → Disconnecting → Idle → `mediaDisconnected` emitted, levels reset to 0 |
| `muteUnmute` | `setMuted(true/false)` emits `mutedChanged`, reflects in `isMuted()` and `SipCall::isMuted()` |
| `deviceSwitchDuringCall` | nonexistent device IDs produce warn log, no crash, media still active |
| `noCrashWithoutDevices` | queried before any call; all accessors return safe defaults |
| `activeCallCleanup` | explicit `detachCall()` mid-call emits `mediaDisconnected`, resets state |

## Known Limitations

- PJSIP audio bridge was tested through compilation only; live end-to-end audio requires `ENABLE_PJSIP=ON` with a real PJSIP installation.
- Hot-device-swap during an active call: selection is persisted (takes effect on next call). PJSIP integer device-index mapping from Qt Multimedia string IDs is not yet implemented.
- Audio level monitoring uses pjsua C API (`pjsua_conf_get_signal_level`); values 0–255 mapped to 0–100. Actual levels may differ slightly from displayed signal strength.
- Level timer emits 0 in stub mode. The meter always shows 0 in tests.
- `CRED_PERSIST_LOCAL_MACHINE` and all prior limitations from Task 17 still apply.
