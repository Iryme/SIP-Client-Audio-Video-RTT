# Media — Audio and Video

**Status:** PARTIAL — Task 9 (device enumeration implemented; streaming deferred)

## Overview

The media layer is responsible for audio/video device enumeration, selection persistence,
and (future) RTP streaming. Task 9 implements the device enumeration and GUI selector
layer. Real capture and playback are deferred until PJSIP integration.

---

## Architecture

```
MediaPanel (src/gui/panels/MediaPanel)
  │
  └── MediaDeviceSelectionModel (src/media/MediaDeviceSelectionModel)
        │  — handles selection, fallback, QSettings persistence
        └── MediaDeviceManager (src/media/MediaDeviceManager)
              │  — singleton, owns the backend
              └── IMediaDeviceBackend
                    ├── QtMediaDeviceBackend  — QMediaDevices (production)
                    └── StubMediaDeviceBackend — controllable list (tests)
```

---

## MediaDevice Model

`src/media/MediaDevice.h`

| Field | Type | Description |
|---|---|---|
| `id` | `QString` | Platform device id (from `QAudioDevice::id()` / `QCameraDevice::id()`) |
| `displayName` | `QString` | Human-readable name |
| `type` | `MediaDeviceType` | `Microphone`, `Speaker`, or `Camera` |
| `isDefault` | `bool` | True if this is the OS-selected default |
| `isAvailable` | `bool` | Always true in current enumeration; reserved for future disconnect tracking |

`isNull()` returns `true` when `id` is empty (used as a sentinel for "no device").

---

## MediaDeviceManager

`src/media/MediaDeviceManager` — singleton.

| Method | Description |
|---|---|
| `listMicrophones()` | Returns all microphone devices |
| `listSpeakers()` | Returns all speaker/output devices |
| `listCameras()` | Returns all camera devices |
| `defaultMicrophone()` | Returns the OS-default microphone (first `isDefault == true`, or first in list) |
| `defaultSpeaker()` | Returns the OS-default speaker |
| `defaultCamera()` | Returns the OS-default camera |
| `refreshDevices()` | Re-enumerates all devices; emits `devicesChanged()` |
| `findDevice(type, id)` | Looks up a device by type and id; returns null device if not found |
| `setBackend(backend)` | Injects a backend (tests only) |

Logging (category `Media`):

| Event | Level |
|---|---|
| Devices enumerated | INFO |
| Selected device missing, falling back | WARN |
| Microphone/speaker/camera selected | INFO |

---

## MediaDeviceSelectionModel

`src/media/MediaDeviceSelectionModel` — handles selection persistence and fallback.

Persisted in `AppSettings` (`SIPClient.ini`):

| Key | Description |
|---|---|
| `media/device/microphone` | Selected microphone id |
| `media/device/speaker` | Selected speaker id |
| `media/device/camera` | Selected camera id |

**Fallback rule:** if the persisted id is missing from the current device list, the model
falls back to the OS default and logs a WARN. It does NOT overwrite the persisted id —
the original selection is preserved in case the device reappears later.

---

## GUI Integration

`MediaPanel` (`src/gui/panels/MediaPanel`) is mounted in the **Media** tab of the
info tab strip below the video area in `MainWindow`.

| Widget | Object name | Function |
|---|---|---|
| `QComboBox` | `MicrophoneCombo` | Lists microphones; shows "(default)" suffix |
| `QComboBox` | `SpeakerCombo` | Lists speakers |
| `QComboBox` | `CameraCombo` | Lists cameras |
| `QPushButton` | `RefreshDevicesBtn` | Calls `MediaDeviceManager::refreshDevices()` |
| `QLabel` | `MicLevelMeter` | Placeholder audio level bar (inactive) |
| `QLabel` | `SpeakerLevelMeter` | Placeholder audio level bar (inactive) |

Combos are disabled with "(no device available)" when the relevant list is empty.

---

## Settings Persistence

Three new keys in `AppSettings` (typed accessors in `src/core/AppSettings.h`):

```cpp
AppSettings::saveSelectedMicrophone(id);
AppSettings::loadSelectedMicrophone();
AppSettings::saveSelectedSpeaker(id);
AppSettings::loadSelectedSpeaker();
AppSettings::saveSelectedCamera(id);
AppSettings::loadSelectedCamera();
```

---

## Audio

- Input: microphone device (selected in Media tab)
- Output: speaker/headset device (selected in Media tab)
- Codecs: OPUS, G.711 (PCMU/PCMA), G.722 — planned with PJSIP integration
- PJSIP handles RTP/RTCP, jitter buffer, echo cancellation

## Video

- Input: camera device (selected in Media tab)
- Codecs: VP8, VP9, H.264 — planned with PJSIP integration
- Remote video rendered in `VideoPanel` via Qt Multimedia video surface
- Local preview rendered in the PiP widget inside `VideoPanel`

## Statistics

Statistics tab shows (future):
- Jitter (ms)
- Packet loss (%)
- RTT latency (ms)
- Audio bitrate (kbps)
- Video bitrate (kbps)
- RTP stream status (active/inactive)

---

## Unit Tests

File: `tests/test_media_device.cpp`
Test QSettings org/app: `IrymeTest` / `SIPClientTest_MediaDevice`

| # | Name | What it verifies |
|---|---|---|
| 1 | `fallbackToDefaultWhenSelectedMissing` | Persisted id not in device list → resolved to default |
| 2 | `selectedDevicePersistence` | `selectMicrophone(id)` persists id; re-resolve returns same id |
| 3 | `emptyDeviceListHandling` | No devices → `selectedMicrophone/Speaker/Camera()` return null |
| 4 | `deviceTypeFiltering` | `findDevice(type, id)` returns null when type does not match |

---

## Known Limitations

- Audio level meters are static/inactive — real capture is deferred to PJSIP integration
- `QMediaDevices::audioOutputs()` may return no devices on virtual machines or headless systems
- Camera enumeration relies on `QCameraDevice` — requires Qt Multimedia camera permissions on macOS
- `devicesChanged()` signal from `QMediaDevices` is not yet forwarded to `MediaDeviceManager` (hot-plug not yet wired)
- Video preview in `VideoPanel` is still a placeholder paintEvent — no real camera stream
