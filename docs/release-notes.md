# Release Notes

See [versioning-and-rollout.md](versioning-and-rollout.md) for the versioning policy, rollout gate, and branch model.

---

## v1.5.0 — Conversation Workspace

**Status:** complete
**Branch:** `feature/w112-conversation-workspace`
**Version bump type:** MINOR
**Scope:** First task of the W112–W117 product-stabilization roadmap. Inverts
the Clients page from call-centric to Contact → Conversation → Messaging →
Call. Also reconciles a real version-source drift found during this task's
audit — see below.

### Version reconciliation

`CMakeLists.txt`'s `project(VERSION)` had been frozen at `0.1.0` since the
project's early skeleton, disconnected from the real release history
(`v1.2.0`…`v1.4.1` tags, this file's own entries). `Application.cpp` also
carried two independent hardcoded `"0.1.0"` string literals never wired to
CMake at all. This release bumps from the real baseline (`1.4.1`) to `1.5.0`
and wires `Application.cpp`'s version reporting to the generated
`AppVersion.h` so it can't drift again. The `v1.4.1` entry above still shows
"Status: in development" despite a `v1.4.1` tag already existing — a
pre-existing inconsistency this task flags but does not silently rewrite.

### Features

**Conversation Workspace (`src/gui/panels/ConversationWorkspacePanel.*`, new)**
- A searchable, sortable conversation list becomes the primary surface in
  the Clients page — pinned conversations first, then most-recent-activity
  first. Rows are the union of saved contacts (`ContactStore`) and peers
  with message history (`ConversationModel`, Task W111), so a saved contact
  with no messages yet still shows up.
- Each row shows last-message preview, timestamp, unread count, presence,
  remote typing state, actual transport of the last sent message, and the
  peer's call state when it's the currently active call.
- Selecting a row drives the existing `ClientMessagingView`
  (`setPeerUri()`) and the dial-target field; a "Call" button starts a call
  from the conversation (`SipManager::makeCall()`) rather than the previous
  dial-first model. A "Pin" toggle persists per-conversation (new
  `AppSettings` keys, contact-level preference).

**Unread tracking (`ConversationModel`, extended)**
- New `unreadCountFor()`/`markRead()` — an in-memory, session-only read
  cursor (deliberately not persisted: `MessageHistoryStore` itself resets
  every app restart, so a cross-restart cursor referencing its entry ids
  would be meaningless).

### Validation

- Build Debug: PASS (MSVC/NMake, `ENABLE_PJSIP=ON`)
- Build Release: PASS
- ctest Debug: 79/79 passed (78 baseline + new `test_conversation_list_model`;
  `test_conversation_model` extended in place)
- ctest Release: 79/79 passed
- Manual GUI pass: app launch/idle smoke PASS; full interactive
  click-through of the new workspace NOT RUN this session (no live SIP peer
  or input-automation tooling available) — see
  `docs/agent-results/W112-conversation-workspace-result.md`.

---

## v1.4.1 — Release Validation Bug Fixes

**Status:** in development
**Branch:** `release/v1.4.0`
**Version bump type:** PATCH
**Scope:** Fixes for the release-blocking bugs found during v1.4.0 manual validation. No new features, no unrelated refactoring.

### Fixes

**Hold/Resume preserves video (`src/sip/SipCall.cpp`)**
- The negotiated video state is now remembered when the local hold is sent (`videoActiveBeforeHold`); the hold renegotiation clears the live availability flags, so resume could no longer see that video had been active and sent the unhold re-INVITE with `videoCount=0` (`m=video 0`) — the call came back audio-only.
- The resume pre-check now also treats video streams in `LOCAL_HOLD` / `REMOTE_HOLD` status as negotiated (previously only `ACTIVE` counted).
- The unhold re-INVITE now offers video as `ENCODING_DECODING` (bidirectional) instead of `DECODING` (receive-only), so the local camera resumes transmitting.
- Remote resume: with the corrected offer/answer on both ends the video stream is restored by the existing media-state handling (which already re-attaches windows and restarts transmit when video becomes ACTIVE again).

**Camera On/Off during an active video call (`src/sip/SipCall.cpp`)**
- `setVideoMuted()` no longer depends on the cached `callVideoMedia` pointer, which goes stale across renegotiations (hold/resume, video re-INVITE) and caused every toggle to be ignored with "no active PJSIP video stream".
- The current video stream index is now queried live via `pjsua_call_get_vid_stream_idx()` and `PJSUA_CALL_VID_STRM_STOP_TRANSMIT` / `START_TRANSMIT` are issued against that index, so the toggle works after any renegotiation.

**Microphone mute (`src/sip/SipCall.cpp`)**
- Mute is now deterministic: the microphone's conference-bridge connection to the call is disconnected (`stopTransmit`) on mute and reconnected (`startTransmit`, with the configured mic volume level re-applied) on unmute, instead of relying on level adjustment.
- The media-state rewiring path now honours an active mute — previously every renegotiation (hold/resume, adding video/RTT) unconditionally reconnected the microphone, silently unmuting the call.
- Speaker/microphone volume paths are unchanged.

**Video aspect ratio (`src/gui/panels/VideoPanel.cpp`)**
- Remote video frames are letterboxed (`Qt::KeepAspectRatio`, centered, black bars) instead of being stretched over the whole panel.
- Embedded PJSIP video HWNDs are letterboxed to the negotiated video resolution instead of being stretched to the full client area.

**Status cards show real call data (`src/gui/panels/CallPanel.cpp`, `src/gui/MainWindow.cpp`)**
- Video Codec / Bitrate / Resolution now show the values actually negotiated for the active call (falling back to the configured settings only while negotiation is pending).
- Audio Codec shows the negotiated codec (e.g. `PCMA/8000`).
- Jitter and Latency are populated from live RTCP stats (`SipManager::currentRtpStats()`, updated via `rtpStatsChanged`); the Clients-page Packet Loss card shows the RTCP loss percentage. Values remain "—" when the backend reports no stats — never invented.

**Diagnostics real values (`src/core/DiagnosticsCollector.cpp`, `src/sip/SipManager.*`, `src/sip/SipCall.*`)**
- Call-ID: the SIP Call-ID of the active call (N/A when idle).
- Dialog state: the PJSIP invite-session state (e.g. `CONFIRMED`; N/A when idle).
- Local IP / local port: the bound SIP transport address (`Endpoint::transportGetInfo`).
- Remote IP / remote port: the remote RTP address of the negotiated audio stream.
- ICE / STUN / TURN remain N/A (not configured in this codebase).

**VideoPanel attach retry loop (`src/gui/panels/VideoPanel.cpp`)**
- The 3-second re-attach timer now stops after a successful attach (previously it re-attached the video windows every 3 seconds for the entire call, flooding the logs and churning the render pipeline) and gives up after 10 failed attempts.
- `attachVideoWindows()` / `attachVideoToWidgets()` now report success so callers can stop retrying.

**Remote `set_win` race (`src/sip/SipCall.cpp`)**
- `pjsua_vid_win_set_win` is no longer called on a window id that `pjsua_vid_win_get_info` cannot verify (PJSIP creates the incoming render window lazily; the first media callback can deliver an id whose window does not exist yet — status 70004). The attach is deferred and reported as incomplete, and the retry path completes it deterministically.

### Additional stabilization fixes (post `8db1001`)

**RTT transcript preserves whitespace (`src/rtt/RttSession.cpp`, `src/gui/panels/RttPanel.cpp`)**
- Incoming T.140 text is no longer trimmed before being checked for emptiness. Spaces between words and the CR/LF that flushes a line to the transcript are real payload; only genuinely empty keepalive packets are suppressed.

**Calls default to audio-only (`src/sip/SipCallOptions.h`, `src/sip/SipManager.cpp`)**
- `SipCallOptions` defaults changed to `requireRtt = false`, `allowVideo = false` — a call is audio-only unless video/RTT are explicitly selected.
- `SipManager::makeCall()` now maps the caller's selected call type (`CallMediaOptions`) onto per-call `SipCallOptions` and places the INVITE via `makeCallWithOptions()`, so the SDP offer matches what the user actually picked instead of always offering audio+video+RTT.

**Request Video / Request RTT drive the real SDP offer (`src/sip/SipCall.cpp`, `src/sip/SipManager.cpp`)**
- Incoming video re-INVITE offers are now detected from the parsed `pjmedia_sdp_session` (via `prm.offer.pjSdpSession`) instead of a substring search on `wholeSdp`, which is empty on some pjsua2 callback paths and cannot distinguish a real offer from a disabled stream (`m=video 0`).
- `answerCall()` now calls `applyVideoSettingsForCall()` before answering: a prior audio-only outgoing call had zeroed all video codec priorities, which silently prevented a subsequent incoming video offer from being negotiated.

**Camera Off no longer leaves a frozen frame (`src/gui/panels/VideoPanel.cpp`, `src/media/PjsipGdiRenderer.cpp`)**
- The local preview widget is blanked (with a "Camera Off" placeholder) instead of retaining the last rendered frame when the camera is disabled mid-call.
- A remote stale-frame watchdog blanks the remote view to black if no decoded frame has arrived in 2 seconds (e.g. the peer's camera goes off), instead of leaving the last received frame frozen on screen. Each frame delivered by the PJSIP GDI renderer is now timestamped (`_pjFrameTs`) so the watchdog can detect staleness.

**Camera On reattaches the preview correctly (`src/sip/SipCall.cpp`, `src/gui/panels/VideoPanel.cpp`)**
- The local preview device now defaults to the same Qt GDI renderer used by `attachVideoWindows()` (`PjsipGdiRenderer::deviceIndex()`), so a preview restarted by Camera On can be re-embedded into the local PiP widget.
- `attachVideoWindows()` now actually rebinds an already-running preview's render target via `pjsua_vid_win_set_win()` (previously a no-op stub left `st = PJ_SUCCESS` without calling it, so frames went nowhere after the capture device was reopened).
- `VideoPanel` re-attaches the embedded preview ~400 ms after `CameraController::enabledChanged(true)` fires mid-call, retrying via the existing attach-retry path on failure.

**New `AudioLevelMeter` widget (`src/gui/widgets/AudioLevelMeter.h/.cpp`)**
- Replaces the plain `QProgressBar` mic/speaker meters in `CallPanel` and the Clients-page status area (`MainWindow`) with a level meter that shades green→red with level, wired to `AudioMediaManager::inputLevelChanged` / `outputLevelChanged`.

**Diagnostics device names fall back to PJSIP (`src/core/DiagnosticsCollector.cpp`)**
- When Qt's device enumeration returns an empty list (seen with RDP-redirected audio devices) and no persisted selection resolves to a device, the microphone/speaker name is now taken from `PjsipAudioMapper::activeCaptureDeviceName()` / `activePlaybackDeviceName()` instead of reporting "N/A".

**RTP TX packet count in Diagnostics (`src/media/RtpStats.h`, `src/core/DiagnosticsCollector.cpp`)**
- `RtpStatsSnapshot` gained `packetsTxAvailable`/`packetsTx` (from `stat.rtcp.txStat.pkt`), surfaced in the diagnostics snapshot alongside the existing RX packet count.

**Pause/Resume preserves RTT across hold (`src/sip/SipCall.cpp`)**
- `rttActiveBeforeHold` is now recorded when local hold is sent, mirroring the existing `videoActiveBeforeHold` handling — the hold renegotiation deactivates the text stream, so the live `rttMediaActive` flag alone would drop RTT from the unhold re-INVITE. The resume path also treats a text stream in `LOCAL_HOLD`/`REMOTE_HOLD` status as negotiated.

### Additional stabilization fixes (round 3)

**RTT dropped when starting/accepting video mid-call (`src/sip/SipCall.cpp`)**
- `requestVideo(true)` only preserved RTT if the text stream was already `PJSUA_CALL_MEDIA_ACTIVE` at the moment the re-INVITE was built. A held text stream (`LOCAL_HOLD`/`REMOTE_HOLD`), an incoming RTT request still awaiting local accept, or a local RTT request still negotiating were all missed, so adding video silently sent `m=text 0` and tore down RTT. The check now also covers held status and both pending states (new `rttRequestPendingLocal` flag, mirroring the existing `videoRequestPendingLocal`), and logs `Preserve RTT during re-INVITE: yes/no reason=...` so the decision is auditable from the logs.
- `requestRtt()` had the matching gap in the other direction (video preservation only checked live-active status) — fixed the same way, and it now sets/clears `rttRequestPendingLocal` and logs `Request RTT ON: textCount=...` / `Accept RTT: textCount=...` depending on whether an incoming request was pending.
- Media-state logging was tightened: every text stream update now logs `RTT media stream status: index=... status=... direction=...`, and the connect/disconnect transitions now log `RTT negotiated active` / `RTT inactive/rejected/withdrawn` (previously less specific wording) plus `Initial call media offer: audio=... video=... rtt=...` on every outbound call.

**Call type selector persistence and visibility (`src/gui/panels/CallPanel.*`, `src/core/AppSettings.h`)**
- The existing "Call type" combo (Audio only / Audio+Video / Audio+RTT / Audio+Video+RTT / RTT only) now persists the last selection (`AppSettings::saveLastCallType`/`loadLastCallType`) and restores it on next launch, defaulting to Audio only when unset.
- A new "Initial Offer" status card shows exactly what the outbound INVITE offered (e.g. "Initial offer: Audio + RTT"), so the UI reflects the actual SDP offer rather than only the combo's current selection.

**Raw SIP/SDP capture for the SIP Ladder (`src/sip/PjsipTraceModule.*`, `src/sip/SipRawMessageParser.*`, `src/sip/SipTraceLogger.cpp`, `src/gui/SipMessageDetailsDialog.*`)**
- Root cause: `SipManager` only ever logged synthetic per-action trace summaries (method/from/to/Call-ID) — no code path captured actual wire-level SIP text, so the SIP Ladder detail dialog (which already existed, fully built to show raw SIP and an extracted body) had nothing real to display.
- Added a `pjsip_module` (`PjsipTraceModule`) registered on the PJSIP endpoint that hooks `on_tx_request`/`on_tx_response`/`on_rx_request`/`on_rx_response`, capturing the full request-line/status-line + headers + body (via `pjsip_tx_data_encode()` for outbound, `rdata->msg_info.msg_buf` for inbound) for every SIP transaction — INVITE, UPDATE, re-INVITE, ACK, BYE, CANCEL, REGISTER, OPTIONS, and all 1xx/2xx/4xx/5xx responses — and forwards it to `SipTraceLogger` (redacted, marshalled to the Qt main thread).
- Added `SipRawMessageParser`, a pure-Qt/text parser (no PJSIP types) that extracts method/status/Call-ID/CSeq/From/To/Content-Type from the raw text, kept separate so it is unit-testable without a live PJSIP stack.
- `SipMessageTrace` gained a `contentType` field; `SipMessageDetailsDialog` now shows it and labels the extracted body "SDP:" when `Content-Type: application/sdp`.
- `SipTraceLogger::exportToJson()`/`exportToText()` previously omitted `rawSip` entirely — both now include the redacted raw SIP text (Authorization/Proxy-Authorization values already stripped by `logMessage()` before storage), so `sip_trace.json`/`sip_trace.txt` in the diagnostics bundle carry real SIP content.

### Validation

- Build Debug: PASS (`cmake --build build`, MSVC/NMake, real PJSIP backend)
- Build Release: PASS (`cmake --build build-release --config Release`, real PJSIP backend)
- ctest Debug: 33/33 passed (includes new `test_sip_raw_message_parser` and extended `test_sip_trace`)
- ctest Release: 33/33 passed
- Windows package regenerated: `dist/SIP-Client-Audio-Video-RTT-v1.4.1-windows.zip` (62,156,993 bytes); smoke test PASS (`SIP backend initialized (PJSIP/pjsua2)`, clean shutdown, no missing DLLs)

---

## v1.4.0 — Call History Foundation

**Status:** in development
**Branch:** `release/v1.4.0`
**Version bump type:** MINOR
**Scope (initial):** Call History foundation — model, persistence, call-lifecycle recording, a Call History page, and a Dashboard summary widget. No LMPE/MSRP/SIP MESSAGE work in this scope.

### Changes

**Model — `CallHistoryEntry` (`src/core/CallHistoryEntry.h/.cpp`)**
- Fields: `id`, `direction` (incoming/outgoing), `remoteUri`, `displayName`, `profileId`/`profileName`, `startTime`, `answerTime`, `endTime`, `durationSec`, `result` (pending/completed/missed/rejected/failed/cancelled), `hadAudio`/`hadVideo`/`hadRtt`, `lastSipCode`, `reason`, `notes`.
- No passwords or auth headers are ever stored on an entry.

**Persistence — `CallHistoryStore` (`src/core/CallHistoryStore.h/.cpp`)**
- JSON array persisted to `call_history.json` in `QStandardPaths::AppDataLocation`, capped at the 500 most recent entries (oldest dropped past that limit).
- Writes are coalesced onto a short timer so bursts of updates (answer immediately followed by end) don't hit disk repeatedly.
- `exportToJson()` writes the full history to an arbitrary path for the Export JSON action.
- Testable via a second constructor that takes an explicit file path, so unit tests never touch the real user's AppData.

**Call lifecycle integration — `CallHistoryRecorder` (`src/core/CallHistoryRecorder.h/.cpp`)**
- Separate from `SipManager`/`MainWindow`; listens to `SipManager`'s existing signals (`incomingCall`, `callStateChanged`, `callDisconnected`, `callFailed`, `audioMediaConnected`, `videoMediaConnected`, `rttMediaConnected`) and drives `CallHistoryStore`.
- Outgoing call created → entry (Outgoing, Pending); incoming call received → entry (Incoming, Pending); state reaches Active → `answerTime` set; call ends → `endTime`/`durationSec`/`result` set.
- Result classification: answered → Completed; unanswered incoming with SIP 486 (local reject, see `SipCall::reject()`) → Rejected; unanswered incoming otherwise → Missed; unanswered outgoing → Cancelled; any `callFailed` → Failed.

**UI — Call History page (`src/gui/panels/CallHistoryPanel.h/.cpp`)**
- New "History" tab in the main navigation rail, alongside Dashboard/Clients/SIP Ladder/Logs/Settings.
- List shows direction, name/URI, date/time, duration, result, and audio/video/RTT badges.
- Clear History (with confirmation dialog) and Export JSON actions; double-click/activate an entry opens a details dialog.

**Dashboard summary**
- New "CALL HISTORY" section in the existing Dashboard statistics panel: Calls Today, Missed Today, Last Call — updates live from `CallHistoryStore::historyChanged`.

**Diagnostics Center — Timeline (`src/core/DiagnosticsTimeline*`)**
- New "Timeline" tab: a unified, filterable, searchable log of registration/call/SIP/media/audio/video/RTT/RTP/camera events plus warning/error entries surfaced from the Logger, capped at the 5000 most recent entries.
- Persisted to `timeline.json` in `QStandardPaths::AppDataLocation` (coalesced writes); Overview tab gets a "Recent Activity" summary of the last 5 entries.
- Export to JSON/TXT from the Timeline tab toolbar.

**Diagnostics Center — Bundle ZIP export (`src/core/DiagnosticsBundleExporter.h/.cpp`)**
- "Generate Diagnostics Bundle" now prompts for a save location (`QFileDialog`, default `diagnostics-YYYYMMDD-HHMMSS.zip` under Documents) and produces a real ZIP archive via Qt's private `QZipWriter` (`Qt6::CorePrivate`, guarded behind `HAVE_QT_ZIP_WRITER`); falls back to a plain folder if that module isn't available in a given Qt build.
- Bundle contents: `diagnostics.json`, `timeline.json`/`timeline.txt` (last 5000 events), `call_history.json`/`call_history.csv`, `logs.txt`, `sip_trace.txt`/`sip_trace.json` (text-only fallback when no real SIP trace exists — never a fabricated ladder), `settings_redacted.json` (AppSettings + SIP profiles, both redacted), `media_devices.json`, `system_info.json`, `version.txt`.
- Redaction: any settings/profile key containing `password`, `secret`, `token`, `authorization`, `auth`, or `credential` (case-insensitive) is replaced with `[REDACTED]`; SIP profile export always marks `password`/`authHeaders` as redacted even though neither is ever stored on `SipProfile` in the first place.
- Diagnostics Center UI: Success/Failure status label next to the button and an "Open Folder" action that jumps to the produced ZIP's (or fallback folder's) location.

### Tests added

`tests/test_call_history.cpp` (`test_call_history`):
- `CallHistoryEntry` JSON round-trip.
- Outgoing / incoming entry creation.
- Completed call duration calculation.
- Missed call (unanswered incoming, non-486 termination).
- Rejected call (unanswered incoming, SIP 486).
- 500-entry cap (oldest entries dropped, newest retained).
- Persist/load round-trip via `exportToJson()` and a second store instance reading the same file.

`tests/test_diagnostics_timeline.cpp` (`test_diagnostics_timeline`):
- Append ordering and 5000-entry cap (oldest dropped).
- Search and category filters.
- JSON round-trip and JSON/TXT export string + file output.

`tests/test_diagnostics_bundle.cpp` (`test_diagnostics_bundle`):
- Redaction key matching (`isSensitiveKey`).
- Bundle manifest contains every expected file.
- Timeline and call history content are present and well-formed in the bundle.
- Settings redaction: a sensitive key's value never appears in `settings_redacted.json`.
- Fallback text when no real SIP trace exists (`sip_trace.txt` says so; `sip_trace.json` is omitted rather than invented).

### Constraints respected

- No LMPE/MSRP/SIP MESSAGE work introduced.
- No credentials, passwords, or auth headers stored in call history.
- Registration logic and existing call flow unchanged.
- Non-blocking persistence (coalesced, small JSON writes on the main thread — no new heavy dependency).

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
| `f6c8d24` | Polish media configuration controls |

### Changes

**Settings → Media tab**
- New tab in Settings (next to Video), implemented in `MediaSettingsPanel`.
- Grouped Microphone / Speaker sections: device dropdown, live level meter, volume slider, and (for the speaker) a Test Speaker button.

**Microphone / speaker device selection**
- Dropdowns populated from real enumerated devices (`MediaDeviceManager::listMicrophones()/listSpeakers()`), with a "Default (system)" entry.
- Selecting a device calls `AudioMediaManager::setMicrophone()/setSpeaker()` — the same API already used by CallPanel — so both surfaces stay consistent.
- Fixed `AudioMediaManager::setMicrophone()/setSpeaker()` to treat an empty device id as "use the system default" instead of rejecting it as an unknown device — this was silently failing before (picking "Default (system)" in either combo, or Reset to Default, did nothing).
- Refresh Devices button triggers `MediaDeviceManager::refreshDevices()` (async Qt Multimedia re-enumeration) and preserves the current selection if the device is still present.
- Missing/disappeared device: falls back to the default via the existing `MediaDeviceSelectionModel` resolution logic, with a warning logged and surfaced in the UI (disabled combo + tooltip when no devices exist at all).

**Reset to Default**
- New "Reset to Default" button in Settings → Media. Sets microphone and speaker device to "Default (system)" and both volumes to 100%, applied immediately via `AudioMediaManager` and persisted to QSettings, same as any other device/volume change.

**Device status labels and fallback warnings**
- Each section now shows a status line: the currently selected device ("Default (system)" or the specific device name), plus the PJSIP-reported "Active now" device when a call's audio media is up (`PjsipAudioMapper::activeCaptureDeviceName()/activePlaybackDeviceName()`, new).
- A distinct warning banner appears when the *previously selected* device has disappeared (vs. no devices at all): "Previously selected microphone/speaker is no longer connected. Reverted to Default (system)."
- Device hot-refresh keeps the current selection if still present, reverts to default with a visible warning if not, and never touches PJSIP call state directly, so it cannot crash an active call.

**Test Microphone**
- New "Test Microphone" button. Highlights the existing live input meter for 10 seconds (or until Stop Test) and shows "Speak now — input meter should move". Sends nothing over SIP and does not fabricate any level — the meter still only reflects real values from `AudioMediaManager::inputLevelChanged`, which requires an active call's audio media to move.

**Real volume control via PJSIP**
- New `AudioMediaManager::setMicrophoneVolume()/setSpeakerVolume()` and `SipCall::setMicVolume()/setSpeakerVolume()`, previously entirely absent from the codebase.
- Microphone gain: `pj::AudDevManager::getCaptureDevMedia().adjustTxLevel(percent / 100.0f)`.
- Speaker gain: `pj::AudDevManager::getPlaybackDevMedia().adjustRxLevel(percent / 100.0f)`.
- Applied immediately to the active call's PJSIP audio media when connected; otherwise stored and applied as soon as audio media connects on the next/current call.

**Persistence in QSettings**
- Keys `media/volume/microphone` and `media/volume/speaker` (default 100 = unity gain) alongside the existing `media/device/microphone`/`media/device/speaker` keys, loaded at startup via `AppSettings`.

**CallPanel sync**
- CallPanel's microphone/speaker volume sliders — previously always disabled with a "not available in current backend" tooltip — are now enabled and wired to the same `AudioMediaManager` API.
- CallPanel's device combos now also refresh on `AudioMediaManager::audioDeviceSelectionChanged`, so a device change (or Reset to Default) made in Settings shows up in CallPanel immediately, not just on the next manual interaction.
- Changing volume or device in either Settings or CallPanel updates the other live (bidirectional), via `AudioMediaManager`'s `microphoneVolumeChanged`/`speakerVolumeChanged`/`audioDeviceSelectionChanged` signals.

**Test Speaker**
- Real playback (not simulated): generates a short 440 Hz sine tone and plays it via `QAudioSink` on the currently selected output device.

**Mute restores user volume**
- Fixed `SipCall::setMuted()`, which previously hardcoded the unmute level to `1.0f` (ignoring any user-configured microphone volume). Unmuting now restores the persisted `micVolume` gain instead of resetting it to full.

**Tooltips**
- Every Media tab control (device combos, volume sliders, Refresh Devices, Reset to Default, Test Speaker, Test Microphone, level meters) now has a tooltip stating what it does, whether it applies immediately or on the next call, and why it's disabled when applicable.

### Manual validation checklist

- [ ] Settings → Media tab shows Microphone and Speaker sections with device dropdown, status line, live meter, and volume slider.
- [ ] Selecting "Default (system)" in either combo actually persists and applies the system default (previously a no-op).
- [ ] Reset to Default sets both devices to Default (system) and both volumes to 100%, applied immediately and persisted.
- [ ] Changing microphone/speaker in Settings updates the active call's audio device without crashing.
- [ ] Changing volume or device in Settings updates CallPanel live, and vice versa.
- [ ] Test Speaker plays an audible tone through the selected output device.
- [ ] Test Microphone highlights the input meter and shows guidance text for 10s or until Stop Test, without sending anything over SIP.
- [ ] Unplugging/removing the selected device falls back to default with a distinct "no longer connected" warning shown in the UI and logged; refresh does not crash during an active call.
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
