# Video Media Integration

**Task 19** — Added in branch `feature/video-media`.

## Overview

Video media integration enables bidirectional SIP video calls when PJSIP is available (`ENABLE_PJSIP=ON`). In stub mode (default, `ENABLE_PJSIP=OFF`) the call state machine works fully; actual video capture/rendering is skipped and the VideoPanel shows a placeholder tint to indicate that the call-state side of video is active.

## Architecture

```
MediaDeviceManager          VideoMediaManager
  (camera enumeration)  ──▶ (application-layer coordinator)
                              │  attachCall() / detachCall()
                              │  setVideoMuted() / isVideoMuted()
                              │  isLocalVideoAvailable() / isRemoteVideoAvailable()
                              │
                      ┌───────▼──────────┐
                      │    SipCall        │  (owns CallStateMachine)
                      │  #ifdef HAVE_PJSIP│
                      │  onCallMediaState │  ──▶ pj::VideoMedia
                      │                   │       local capture ◀──▶ network ◀──▶ remote render
                      └──────────────────┘
```

### `VideoMediaManager` (`src/media/VideoMediaManager.h/.cpp`)

Application-layer singleton. Qt-only (no `pjsua2.hpp`).

- `attachCall(SipCall*)` — wires to SipCall video signals; called by SipManager on call creation.
- `detachCall()` — disconnects signals; called by SipManager on call end (Idle/Failed) or shutdown. Always resets `m_videoActive` even when the `QPointer<SipCall>` is already null (mirrors the AudioMediaManager fix).
- `setVideoMuted(bool)` — delegates to `SipCall::setVideoMuted()`; in PJSIP mode calls `vidSetStream(PJSUA_CALL_VID_STRM_STOP_TRANSMIT / START_TRANSMIT)`.
- `setCamera(id)` — persists via `MediaDeviceSelectionModel`; logs diagnostic. Hot-swap during an active call requires PJSIP re-initialisation (scaffolded; takes effect on next call).
- `isVideoActive()`, `isLocalVideoAvailable()`, `isRemoteVideoAvailable()` — state accessors.

### `SipCall` additions

New signals:
- `videoMediaConnected()` — stub: emitted alongside `audioMediaConnected` when state reaches Active; PJSIP: emitted from `onCallMediaState` when a video stream becomes `PJSUA_CALL_MEDIA_ACTIVE`.
- `videoMediaDisconnected()` — emitted when the video bridge is torn down or call ends.
- `localVideoStarted()` / `localVideoStopped()` — fine-grained local-capture availability.
- `remoteVideoStarted()` / `remoteVideoStopped()` — fine-grained remote-stream availability.
- `videoMuteChanged(bool)` — emitted from `setVideoMuted()`.

New methods:
- `setVideoMuted(bool)` — stub: flag + signal; PJSIP: `vidSetStream(STOP/START_TRANSMIT)`.
- `isVideoMuted() const`
- `isLocalVideoAvailable() const`
- `isRemoteVideoAvailable() const`

### PJSIP video bridge (inside `#ifdef HAVE_PJSIP` in `SipCall.cpp`)

`SipCall::Impl::PjCall::onCallMediaState()` now iterates all media streams. When a stream with `PJMEDIA_TYPE_VIDEO` reaches `PJSUA_CALL_MEDIA_ACTIVE`:

```cpp
auto *vid = static_cast<pj::VideoMedia *>(getMedia(mi.index));
m_impl->callVideoMedia = vid;
// emit videoMediaConnected + localVideoStarted + remoteVideoStarted
```

On `PJSIP_INV_STATE_DISCONNECTED`, `stopVideoBridge()` clears `callVideoMedia` and emits the corresponding stopped/disconnected signals.

Real video rendering (attaching a native window handle via `pj::VideoWindow`) is a known future task.

### `SipManager` additions

- `setCallVideoMuted(bool)` / `isCallVideoMuted()` — delegates to active `SipCall`.
- Forwarded signals: `videoMediaConnected`, `videoMediaDisconnected`, `callVideoMuteChanged`, `localVideoStarted`, `localVideoStopped`, `remoteVideoStarted`, `remoteVideoStopped`.
- `makeCall()` and `onAccountIncomingCall()` both call `VideoMediaManager::instance().attachCall(m_activeCall)`.
- `onActiveCallStateChanged()` (Idle/Failed path) and `destroyActiveCall()` both call `VideoMediaManager::instance().detachCall()` — idempotent.

### `VideoPanel` additions

- **Camera selector** (`m_cameraSelector`) — `QComboBox` in a translucent overlay, populated from `MediaDeviceManager`, visible only during an active call. Changes call `VideoMediaManager::setCamera()`.
- **Video mute button** (`m_btnVideoMute`) — in overlay; `toggled` → `VideoMediaManager::setVideoMuted()`; `videoMutedChanged` → button text/state sync with `QSignalBlocker`.
- **Swap button** (`m_btnSwap`) — bottom-centre; toggles `m_swapped` flag, which reverses which half of the label is called "Local" vs "Remote" (placeholder until real `QVideoWidget` rendering is wired).
- **`paintEvent`** — idle: dark crosshair placeholder; video active: lighter tint with "Remote Video" / "Video Muted" / "Local Video" label depending on `m_videoMuted` and `m_swapped`.
- **`applyVideoState()`** — syncs overlay visibility, signal indicator colour/text, PiP label text.

### `CallPanel` additions

- **`m_btnVideo`** (previously a placeholder toggle) is now wired: `toggled` → `VideoMediaManager::setVideoMuted()`; `videoMutedChanged` → `onVideoMuteChanged()` → button text/checked state sync with `QSignalBlocker`.

## Diagnostics

All video lifecycle events are logged at `LogCategory::Media`:

| Event | Message |
|---|---|
| Video media connected | `Video media connected` |
| Video media disconnected | `Video media disconnected` |
| Video media disconnected on detach | `Video media disconnected (call detached)` |
| Local video started | `Local video started` |
| Local video stopped | `Local video stopped` |
| Remote video started | `Remote video started` |
| Remote video stopped | `Remote video stopped` |
| Video mute enabled | `Video mute enabled` |
| Video mute disabled | `Video mute disabled` |
| Camera selected | `Camera selected: <displayName>` |
| Camera changed during call | `Camera changed during active call: <name> (PJSIP hot-swap scaffolded...)` |
| Camera not found | `setCamera: device id '<id>' not found` |

In `SipCall.cpp` at `LogCategory::Sip`:

| Event | Message |
|---|---|
| Video mute ON | `Video mute ON: call id=<id>` |
| Video mute OFF | `Video mute OFF: call id=<id>` |

## Tests (`tests/test_video_media.cpp`)

6 Qt Test cases (`QTEST_GUILESS_MAIN`). All tests bypass SipManager and drive `SipCall` directly via `VideoMediaManager::attachCall()`.

| Test | Description |
|---|---|
| `attachVideoOnActive` | `attachCall` + drive to Active → `videoMediaConnected` emitted, `isVideoActive()` true, local/remote available |
| `detachVideoOnHangup` | hangup → Disconnecting → Idle → `videoMediaDisconnected` emitted, availability reset |
| `videoMuteUnmute` | `setVideoMuted(true/false)` emits `videoMutedChanged`, reflects in `isVideoMuted()` and `SipCall::isVideoMuted()`; idempotent |
| `cameraSwitchDuringCall` | nonexistent camera IDs produce warn log, no crash, video still active |
| `noCrashWithoutCall` | queried before any call; all accessors return safe defaults; double-detach safe |
| `activeCallCleanup` | explicit `detachCall()` mid-call emits `videoMediaDisconnected`, resets state |

## Known Limitations

- PJSIP video bridge was tested through compilation only; live end-to-end video requires `ENABLE_PJSIP=ON` with a real PJSIP installation.
- Real video rendering to a widget requires attaching a native window handle via `pj::VideoWindow` — not yet implemented. `VideoPanel` shows a colour-tinted placeholder.
- Camera hot-swap during a call: selection is persisted (takes effect on next call). PJSIP integer device-index mapping from Qt Multimedia string IDs not yet implemented.
- Local camera preview before a call (without an active SIP session) is not yet started. The `VideoPanel` shows "Camera Off" in the PiP area until the call goes active.
- Video level monitoring is not implemented (no equivalent of the audio `pjsua_conf_get_signal_level` API for video).
- All prior limitations from Task 18 (audio) still apply.
