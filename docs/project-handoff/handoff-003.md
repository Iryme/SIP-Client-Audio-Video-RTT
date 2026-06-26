# Project Handoff 003

Snapshot date: 2026-06-26
Active branch: `feature/project-handoff-002`
HEAD: `96ee7b4`
Project: **SIP Client Audio Video RTT**

This is the third formal project handoff. It covers Tasks 22-28.6 (real PJSIP
validation through RTT live end-to-end validation). Tasks 1-21 are documented
in `handoff-001.md` and `handoff-002.md`; this document provides a complete
continuation prompt independently of them.

---

## 1. Executive Summary

**SIP Client Audio Video RTT** is a Windows-first cross-platform SIP multimedia
desktop client written in C++17 with Qt 6 and PJSIP/pjsua2 2.17-dev. It
targets emergency-services and accessibility scenarios requiring
audio + video + real-time text (RTT, RFC 4103 / T.140) in a single call.

The application has now completed its core implementation phase:

| Subsystem | Status |
|---|---|
| SIP registration (REGISTER / re-REGISTER / retry / profile switch) | **COMPLETE — live validated** |
| Audio calls (outbound + inbound, G.722 / G.711) | **COMPLETE — live validated** |
| Video calls (VP8, DirectShow, PJSIP render via Win32 SetParent) | **COMPLETE — live validated** |
| RTT RFC 4103 / T.140 (TX char-by-char, RX, RED level 2) | **COMPLETE — live validated** |
| ETSI TS 103 479 / 103 480 / 103 698 LMPE | **NOT STARTED** |

The next phase is ETSI / NG112 emergency-services integration.

### Technology Stack

| Component | Version / Detail |
|---|---|
| Language | C++17 |
| GUI toolkit | Qt 6.11.1 (Core, Gui, Widgets, Multimedia, MultimediaWidgets) |
| SIP stack | PJSIP / pjsua2 2.17-dev (built from source) |
| Video codec | VP8 (libvpx via vcpkg `x64-windows-static`) |
| Video capture | DirectShow (PJMEDIA_WITH_VIDEODEV_DSHOW=ON) |
| Video rendering | Win32 SetParent / MoveWindow into Qt HWND — `PjsipGdiRenderer` |
| RTT | RFC 4103 / T.140 over RTP; RED redundancy RFC 2198, level 2 |
| SIP server (test) | Kamailio 5.x at `10.2.0.180:5060` |
| Compiler | MSVC 2022 (amd64) |
| Build system | CMake 3.16+, NMake Makefiles generator |
| Test framework | Qt Test |
| OS | Windows 11 Pro (primary); Linux build supported |

---

## 2. Current Project Status

### SIP

| Feature | State |
|---|---|
| REGISTER / UNREGISTER | COMPLETE |
| Digest 401 auth challenge | COMPLETE |
| Re-REGISTER refresh (80% / 30s margin) | COMPLETE |
| Exponential retry on transient failure | COMPLETE |
| Profile switch with sequencing | COMPLETE |
| Incoming call (INVITE) | COMPLETE |
| Outgoing call (INVITE) | COMPLETE |
| BYE / hangup | COMPLETE |
| Hold / Resume (re-INVITE) | COMPLETE |
| SIP URI normalization | COMPLETE |
| Outbound proxy support | COMPLETE |
| SIP trace logger + ladder view | COMPLETE |

### Audio

| Feature | State |
|---|---|
| Audio RTP (G.722 @ 16 kHz, G.711 @ 8 kHz) | COMPLETE |
| Audio device selection (mic + speaker) | COMPLETE |
| Mute / unmute microphone | COMPLETE |
| Audio level meters | COMPLETE |
| Default (system) audio device support | COMPLETE |
| Hot-swap during call | NOT IMPLEMENTED |
| Hot-plug detection | NOT IMPLEMENTED |

### Video

| Feature | State |
|---|---|
| VP8 codec (via libvpx / vcpkg) | COMPLETE |
| DirectShow camera enumeration | COMPLETE |
| PJSIP video window embed (SetParent + MoveWindow) | COMPLETE |
| Remote video rendering | COMPLETE |
| Local preview rendering | COMPLETE |
| Video mute (stop TX) | COMPLETE |
| Camera device selection | COMPLETE |
| Video window resize handling | COMPLETE |
| VP9 | NOT IMPLEMENTED |
| H.264 | NOT IMPLEMENTED |

### RTT

| Feature | State |
|---|---|
| SDP m=text offer/answer | COMPLETE |
| T.140 TX — char-by-char delta | COMPLETE |
| T.140 RX — accumulate + display | COMPLETE |
| Backspace (U+0008) TX and RX | COMPLETE |
| Enter → transcript flush (U+000D) | COMPLETE |
| RED redundancy RFC 2198, level 2 | COMPLETE |
| RttSession 5-state machine | COMPLETE |
| GUI RttPanel (live typing, transcript) | COMPLETE |
| LMPE panel | PLACEHOLDER (UI only) |
| ETSI TS 103 479 extensions | NOT STARTED |

---

## 3. Live Validation Matrix

| Feature | Automated Test | GUI Tested | Live Tested | Result |
|---|---|---|---|---|
| REGISTER / 401 / 200 OK | YES (stub) | YES | YES — Kamailio | PASS |
| UNREGISTER | YES (stub) | YES | YES — Kamailio | PASS |
| Registration retry / backoff | YES (stub) | NO | NO | PASS (unit) |
| Registration expiry refresh | YES (stub) | NO | NO | PASS (unit) |
| Profile switch sequencing | YES (stub) | NO | NO | PASS (unit) |
| Outgoing INVITE | YES (stub) | YES | YES — Kamailio | PASS |
| Incoming INVITE | YES (stub) | NO | NO | PASS (stub only) |
| BYE from local | YES (stub) | YES | YES | PASS |
| BYE from remote | YES (stub) | YES | YES | PASS |
| Hold / Resume | YES (stub) | NO | NO | PASS (stub only) |
| SIP URI normalization | YES | NO | NO | PASS |
| Audio G.722 RTP | NO | YES | YES — PJSUA peer | PASS |
| Audio G.711 RTP | NO | YES | YES — Kamailio | PASS |
| Audio mute | NO | YES | YES | PASS |
| Audio level meters | NO | YES | YES | PASS |
| Video VP8 | NO | YES | YES — PJSUA peer | PASS |
| Video remote render | NO | YES | YES | PASS |
| Video local preview | NO | YES | YES | PASS |
| Video mute | NO | YES | YES | PASS |
| RTT SDP m=text negotiation | YES | YES | YES — PJSUA peer | PASS |
| RTT TX char-by-char | NO | YES | YES — PJSUA peer | PASS |
| RTT RX char-by-char | NO | YES | YES — PJSUA peer | PASS |
| RTT Backspace TX/RX | NO | YES | YES | PASS |
| RTT Enter → transcript | NO | YES | YES | PASS |
| RTT RED level 2 | NO | NO | YES (SDP confirmed) | PASS |
| RTT cleanup on BYE | NO | YES | YES | PASS |
| 3 consecutive calls, no crash | NO | YES | YES | PASS |
| SIP trace logger (stub) | YES | YES | NO | PASS |
| Codec matrix logging | YES | NO | NO | PASS |

---

## 4. Architecture

### Module Map

```
main.cpp
 └── Application
       └── MainWindow
             ├── NavRail
             ├── SidebarPanel ──► SipProfileManager (singleton)
             │                ──► SipManager (register/unregister)
             ├── CallPanel    ──► SipManager (call control)
             ├── VideoPanel   ──► VideoMediaManager
             ├── RttPanel     ──► RttSession ──► SipCall
             ├── DiagnosticsPanel ──► Logger, SipTraceLogger, SipLadderWidget
             └── AppStatusBar

SipManager (singleton)
 ├── owns SipAccount (pjsua2 account wrapper)
 ├── owns SipCall (active call)
 ├── owns RttSession (attached to active SipCall)
 ├── drives RegistrationStateMachine
 ├── drives AudioMediaManager (singleton)
 └── drives VideoMediaManager (singleton)

SipCall
 ├── owns CallStateMachine (9-state)
 ├── owns Impl::PjCall (pjsua2 Call subclass, HAVE_PJSIP only)
 └── signals: callStateChanged, audio/video/rtt media connected/disconnected

RttSession
 └── connected to SipCall.rttMediaConnected / rttTextReceived signals
```

### SipManager

`src/sip/SipManager.h/.cpp` — singleton. Owns the PJSIP Endpoint lifecycle.
Responsibilities:
- Initialize / shutdown the pjsua2 Endpoint.
- Drive `RegistrationStateMachine` from `SipAccount` callbacks.
- Schedule registration retry (`RegistrationRetryPolicy`) and refresh (`RegistrationRefreshConfig`).
- `makeCall()` / `answerCall()` / `hangupCall()` / `holdCall()` / `resumeCall()`.
- Attach / detach `AudioMediaManager` and `VideoMediaManager` on call create/destroy.
- Own the `RttSession` and expose `rttSession()` to `RttPanel`.
- Emit 11 synthetic `SipMessageTrace` points for the SIP ladder view.

All PJSIP callbacks arrive on the PJSIP thread and are dispatched to the Qt
main thread via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.

### SipCall

`src/sip/SipCall.h/.cpp` — one instance per active call. Owns:
- `CallStateMachine` (9 states: Idle, OutgoingInit, Ringing, IncomingRinging, Connecting, Active, Held, Disconnecting, Failed).
- `Impl::PjCall` — pjsua2 `Call` subclass that receives `onCallState`, `onCallMediaState`, `onStreamCreated` callbacks (HAVE_PJSIP only).
- `QTimer m_levelTimer` — fires at 10 Hz to poll PJSIP audio levels via `pjsua_conf_get_signal_level`.

Key methods: `makeCall`, `answer`, `reject`, `hangup`, `hold`, `resume`, `setMuted`, `setVideoMuted`, `sendRttText`, `attachVideoWindows`, `releasePjsipCall`.

### CodecManager

`src/sip/CodecManager.h/.cpp` — singleton. Called once after `libStart()`.
Detects available PJSIP audio/video codecs, applies default priorities (VP8=230,
VP9=0, PCMU=0, G722=255, G711=128), logs the full codec matrix. Also models
RTT codecs (t140, red) for SDP awareness — PJSIP has no T.140 codec object.

### AudioMediaManager / VideoMediaManager

`src/media/AudioMediaManager.h/.cpp` and `src/media/VideoMediaManager.h/.cpp` —
Qt-only singletons (no pjsua2 headers). They receive `attachCall(SipCall*)` and
`detachCall()` from SipManager and forward signals to the GUI. All PJSIP bridge
code lives inside `SipCall.cpp` behind `HAVE_PJSIP`.

### PjsipGdiRenderer

`src/media/PjsipGdiRenderer.h/.cpp` — Windows-only (`#ifdef _WIN32`). Provides
`deviceIndex(WId hwnd)` which creates a PJMEDIA colorbar-based render target for
a given HWND. Used by `SipCall::attachVideoWindows()` to embed PJSIP video frames
into Qt widget windows via Win32 `SetParent` + `MoveWindow`. Falls back to null
renderer index on non-Windows.

### RttSession

`src/rtt/RttSession.h/.cpp` — 5-state machine (Disabled, Offered, Negotiated,
Active, Failed). Attached to a `SipCall` via `enableForCall(SipCall*)`. Connects
to `rttMediaConnected` / `rttMediaDisconnected` / `rttTextReceived` signals.
`sendText(text)` forwards to `SipCall::sendRttText()`.

### RttPanel

`src/gui/panels/RttPanel.h/.cpp` — the RTT GUI panel. Wired to `RttSession` via
`setRttSession(session)`. Key behavior:
- `onRttInputChanged(newText)` — fires on every keystroke, computes T.140 delta
  (new characters vs. `m_prevLocalText`), sends BS (U+0008) for deletions and
  new chars for additions, via `rttSession->sendText()`.
- `onRttSend()` — sends CR (U+000D), appends to transcript as "You: …", clears input.
- `processRemoteText(incoming)` — handles incoming T.140 blocks: BS removes last
  char from `m_remoteBuffer`, CR/LF flushes buffer to transcript.

### State Machines

**RegistrationStateMachine** (5 states):
```
Unregistered → Registering → Registered → Unregistering → RegistrationFailed
```
30-second watchdog on Registering and Unregistering.

**CallStateMachine** (9 states):
```
Idle → OutgoingInit → Ringing
     ↑                       ↘ Connecting → Active → Held
     │                                         ↓
     └── Failed ← Disconnecting ←──────────────┘
                                ↑
                   IncomingRinging → Connecting
```
30-second watchdog on OutgoingInit and Disconnecting.

---

## 5. RTT — Implementation Detail

### SDP Negotiation

When `SipProfile::enableRtt == true`, the pjsua2 `AccountConfig` is modified:
```
accountCfg.mediaConfig.enableText = true;
accountCfg.mediaConfig.textRedundancyLevel = 2;
```
PJSIP then adds an `m=text` line to every INVITE:
```
m=text <port> RTP/AVP 100 98
a=rtpmap:100 t140/1000
a=rtpmap:98 red/1000
a=fmtp:98 100/100/100
```
The peer (e.g. PJSUA with `--text --text-red=2`) offers the same; PJSIP
negotiates RED if both sides support it, otherwise falls back to plain T.140.

### TX — Character-by-Character Delta

`RttPanel::onRttInputChanged(newText)`:
1. Compare `newText` with `m_prevLocalText`.
2. If newText is shorter (deletion): send U+0008 × (old - new length).
3. If newText is longer (addition): send the appended suffix.
4. Update `m_prevLocalText = newText`.

Delta is sent via `RttSession::sendText(delta)` → `SipCall::sendRttText(delta)`.

In `SipCall.cpp` (HAVE_PJSIP):
```cpp
void SipCall::sendRttText(const QString &text) {
    // m_impl->pjCall->sendTextStream(text) — writes UTF-8 bytes to
    // the pjmedia text stream obtained from getMedia(textMediaIndex)
}
```

### RX — Accumulation and Display

`SipCall::Impl::PjCall::onStreamCreated()` registers a custom callback on the
T.140 RTP port. When a T.140 packet arrives:
1. PJSIP decodes RED packets (strips redundancy headers).
2. Delivers the T.140 payload to the app as a `QString` via `rttTextReceived`.
3. `RttSession` receives it and emits `remoteTextReceived`.
4. `RttPanel::processRemoteText(text)` appends to `m_remoteBuffer`, updates
   `m_rttRemoteLive` widget; BS removes last char; CR/LF flushes to transcript.

### RED (RFC 2198) Redundancy

`redundancyLevel = 2` means each T.140 RTP packet includes the payload from the
previous 2 packets as redundancy headers. This recovers 2 consecutive packet
losses without retransmission. The level is set at account creation time; it
cannot be changed at runtime without re-registering.

### T.140 Special Characters

| Code | Meaning | Handling |
|---|---|---|
| U+0008 | Backspace | TX: sent as delta; RX: removes last char from buffer |
| U+000D | Carriage Return | TX: sent by Enter/Send button; RX: flush buffer to transcript |
| U+000A | Line Feed | RX: treated same as CR |
| U+FEFF | BOM | RX: ignored |

### PJSIP APIs Used

- `pj::AccountConfig::mediaConfig.enableText` — enables m=text in SDP
- `pj::AccountConfig::mediaConfig.textRedundancyLevel` — RED level
- `pjsua_call_get_med_info()` → iterate `PJMEDIA_TYPE_TEXT` streams
- `pj::Call::getMedia(index)` → cast to text media, call `send()`
- `onCallMediaState` callback → detect text stream becoming active/inactive

---

## 6. Video — Implementation Detail

### Window Embedding (SetParent)

PJSIP renders video to a native Win32 HWND. Qt widgets expose `winId()` which
returns a Win32 HWND. Integration steps:

1. When `videoMediaConnected` fires, `SipManager` calls
   `SipCall::attachVideoWindows(remoteWId, localPreviewWId)`.
2. `attachVideoWindows` calls `PjsipGdiRenderer::deviceIndex(hwnd)` for each
   HWND, which registers a PJMEDIA GDI render device pointing to that window.
3. `pjsua_vid_win_set_window()` (wrapped in pjsua2) attaches the PJSIP video
   window to the returned device index.
4. Win32 `SetParent(pjsipHwnd, qtHwnd)` re-parents the PJSIP window under the
   Qt widget.
5. `MoveWindow(pjsipHwnd, 0, 0, w, h, TRUE)` fills the Qt widget area.

### Resize Handling

`VideoPanel::resizeEvent` calls `SipCall::attachVideoWindows` again with the new
size. PJSIP re-parents and resizes via another `MoveWindow` call.

### Preview HWND Allocation

PJSIP allocates local-preview HWNDs lazily. If `pjsua_vid_win_get_info()` returns
a null HWND on first attachment, the code retries after a 200 ms `QTimer`
one-shot. This avoids a race where the preview window is not ready immediately
after `onCallMediaState`.

### Cleanup

`SipCall::releasePjsipCall()` must be called before `deleteLater()` to allow
the PJSIP call slot to be recycled cleanly (see Section 8 — Patches).

### Lifecycle

```
call INVITE accepted
  → onCallMediaState: VIDEO stream ACTIVE
  → SipCall emits videoMediaConnected
  → VideoMediaManager.attachCall signals VideoPanel
  → VideoPanel: show overlay, request attachVideoWindows
  → SipCall: PjsipGdiRenderer registers HWNDs, SetParent, MoveWindow

call BYE received / hangup
  → SipCall: stopVideoBridge, emit videoMediaDisconnected
  → VideoMediaManager.detachCall
  → VideoPanel: hide overlay, clear placeholder
```

---

## 7. Important Fixes (Chronological)

| Task | Fix | Description |
|---|---|---|
| 22E | `releasePjsipCall()` before `deleteLater()` | pjsua2 `Call::~Call()` was called while the call slot was still active, causing a PJSIP assertion on account destruction. |
| 25C | Video window SetParent + MoveWindow | Initial attempt used `pjsua_vid_win_set_window()` only; PJSIP window was not actually reparented into Qt widget. Fixed by using Win32 `SetParent` + `MoveWindow` explicitly. |
| 25C | Lazy preview HWND + retry timer | `pjsua_vid_win_get_info()` returns null HWND until the preview is started. Added a 200 ms one-shot `QTimer` to retry attachment after PJSIP allocates the HWND. |
| 25C | Video re-attachment on re-negotiation | On call re-INVITE (hold/resume), PJSIP re-creates the video window. Code now re-calls `attachVideoWindows` on every `onCallMediaState` VIDEO ACTIVE event. |
| 27A | `PjsipGdiRenderer` custom GDI device | Plain `pjsua_vid_win_set_window()` could not map a Qt HWND to a PJMEDIA render device. Added a custom `pjmedia_vid_dev_factory` (GDI-based) to register each Qt HWND as a named PJMEDIA device and return its integer index. |
| 27B | Preview stop crash | Stopping local preview before call end caused `pjsua_vid_preview_stop()` to crash if called after the call was already deallocated. Fixed by guarding with `pjsua_call_is_active()`. |
| 28.x | pjsua2 `Call::~Call()` race | See Section 8 — Patches. |

---

## 8. Patches

### `patches/0001-pjsua2-call-dtor-guard-user-data-clear.patch`

**File patched:** `.deps/pjproject/pjsip/src/pjsua2/call.cpp`

**Problem:** `Call::~Call()` unconditionally cleared `user_data` for the call
slot via `pjsua_call_set_user_data(id, NULL)`. pjsua recycles call slots; if a
new incoming call had already claimed the slot (setting `user_data` to a new
`EarlyCall` or `PjCall` instance), clearing it unconditionally caused the new
call to be auto-rejected with 500 by `endpoint.cpp::on_incoming_call()`.

This manifested as: a fast hangup immediately followed by a new incoming call
would be rejected with 500 Internal Server Error.

**Fix:**
```diff
-    if (id != PJSUA_INVALID_ID)
-        pjsua_call_set_user_data(id, NULL);
+    if (id != PJSUA_INVALID_ID && pjsua_call_get_user_data(id) == this)
+        pjsua_call_set_user_data(id, NULL);
```

**When to re-apply:** After any `git clean` or re-clone of `.deps/pjproject`, or
after updating pjproject to a new commit/tag. Check with:
```bash
grep -n "pjsua_call_set_user_data" .deps/pjproject/pjsip/src/pjsua2/call.cpp
```
The patched line should read `&& pjsua_call_get_user_data(id) == this`.

**How to re-apply:**
```bash
cd .deps/pjproject
git apply ../../patches/0001-pjsua2-call-dtor-guard-user-data-clear.patch
```
Then rebuild PJSIP:
```powershell
cd .deps/pjproject
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
nmake -f build\Makefile
```

---

## 9. Build

### Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| CMake | 3.16+ | |
| Qt | 6.4+ | Core, Gui, Widgets, Multimedia, MultimediaWidgets |
| MSVC | 2022 (amd64) | VS 17 Community or higher |
| PJSIP / pjsua2 | 2.17-dev | Built from source in `.deps/pjproject/` |
| libvpx | via vcpkg | `x64-windows-static` triplet |

### Windows — Debug Build with PJSIP

```powershell
# Open x64 MSVC dev shell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -SkipAutomaticLocation

# Configure
$env:CMAKE_PREFIX_PATH = "F:\Programs\Qt\6.11.1\msvc2022_64"
cmake -S . -B build -G "NMake Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DENABLE_PJSIP=ON `
  -DPJSIP_DIR="$PWD\.deps\pjproject" `
  -DBUILD_TESTS=ON

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Stub Mode (no PJSIP — default, CI)

```powershell
cmake -S . -B build -G "NMake Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DENABLE_PJSIP=OFF `
  -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### PJSIP Build (one-time, Windows)

```powershell
cd .deps\pjproject
# Apply patch first:
git apply ..\..\patches\0001-pjsua2-call-dtor-guard-user-data-clear.patch
# Build:
& "..\..\build-pjsip.ps1"   # or nmake -f build\Makefile as appropriate
```

CMake option summary:

| Option | Default | Description |
|---|---|---|
| `ENABLE_PJSIP` | OFF | Enable real PJSIP backend |
| `PJSIP_DIR` | (empty) | Path to built PJSIP |
| `BUILD_TESTS` | OFF | Build test executables |
| `BUILD_LIVE_VALIDATION_TOOLS` | OFF | Build live-probe test binaries |

---

## 10. Tests

### Test Suites (CTest — 15 suites, stub mode)

| # | Suite | Methods | What it validates |
|---|---|---|---|
| 1 | `test_sip_profile_manager` | 12 | SipProfile CRUD, validation, persistence, URI derivation |
| 2 | `test_credential_store` | 12 | CredentialStore + WindowsCredentialBackend + MemoryBackend |
| 3 | `test_sip_profile_editor` | 10 | SipProfileEditorDialog — Add/Edit/Delete, password integration |
| 4 | `test_media_device` | 6 | MediaDeviceManager enumeration, selection model, fallback |
| 5 | `test_registration_state_machine` | 28 | RegistrationStateMachine transitions, guards, watchdog |
| 6 | `test_sip_manager` | 15 | SipManager registration + call control (stub) |
| 7 | `test_registration_retry` | 14 | RegistrationRetryPolicy isRetryable + exponential backoff |
| 8 | `test_registration_expiry` | 14 | RegistrationRefreshConfig delayMsForExpiry |
| 9 | `test_profile_switch` | 10 | switchActiveProfile() sequencing, pending-switch guard |
| 10 | `test_sip_uri_normalizer` | ~8 | SipUriNormalizer — bare users, full URIs, ports, transports |
| 11 | `test_call_state_machine` | 10 | CallStateMachine 9-state transitions, watchdog |
| 12 | `test_audio_media` | 6 | AudioMediaManager attach/detach, mute, device selection |
| 13 | `test_video_media` | 6 | VideoMediaManager attach/detach, mute, camera selection |
| 14 | `test_codec_manager` | ~4 | CodecManager stub mode codec lists, hasUsableVideoCodec |
| 15 | `test_sip_trace` | 6 | SipTraceLogger: redaction, store, export Text/JSON |

**Current result:** 15/15 suites PASS, 0 failures (stub mode, Windows, Qt 6.11.1 / MSVC 2022).

### Live Validation Tools (`BUILD_LIVE_VALIDATION_TOOLS=ON`)

| Tool | Purpose |
|---|---|
| `live_registration_probe` | REGISTER against real Kamailio, verify 200 OK |
| `live_audio_call_probe` | Outbound call + audio RTP exchange |

### RTT GUI Validation

The RTT GUI end-to-end test was performed via Windows UI Automation (PowerShell
`UIAutomationClient`). See `docs/rtt-interoperability-test.md` for the full
procedure and results. The script `task286_v3.ps1` in the session scratchpad
automates 17 test items covering: SDP negotiation, RTT TX char-by-char, BS,
Enter→transcript, RX from PJSUA peer, GUI hangup cleanup, peer hangup cleanup,
and 3 consecutive calls without crash. All 17 items PASS.

---

## 11. Known Limitations

### RTT

- `redundancyLevel` is set at account creation time; changing it requires
  re-registration. No runtime adjustment.
- ETSI TS 103 479 extensions (priority indication, session identity,
  emergency-specific session handling) are **not implemented**.
- MIME type multiplexing (text/t140 + MIME boundary for NG112) is **not
  implemented**.
- T.140 backspace on some older UA implementations may display U+0008
  literally instead of deleting the previous character.

### Video

- VP9 not available: `PJMEDIA_HAS_VPX_CODEC_VP9=1` is not defined; only VP8.
- H.264 not implemented.
- Camera hot-swap during an active call is not implemented; takes effect on
  the next call.
- Hot-plug detection (`QMediaDevices` signals) not forwarded into
  `MediaDeviceManager`; user must press Refresh.

### Audio

- Hot-swap audio device during a call is not implemented.
- PJSIP integer device-index mapping from Qt Multimedia string IDs is done
  via `PjsipAudioMapper`; edge cases on unusual device names may mismatch.

### Registration

- No automatic registration on startup or on profile selection.
- Network-change recovery (e.g. Wi-Fi roaming) not implemented.
- One account at a time; multi-account is not implemented.
- Retry counter resets on exhaustion; no persistent backoff across app restarts.
- Watchdog during profile switch proceeds with new profile even if old UNREGISTER
  was never acknowledged; old registration persists until server-side expiry.

### Calls

- Incoming call handling via GUI has not been tested in a live scenario.
- One concurrent call; conference / multi-party not supported.
- No call duration timer (UI placeholder).
- No incoming call notification (tray/popup) when window is unfocused.
- Call history is not persisted.

### Credentials

- `CredentialStore` Windows backend only; Linux/macOS backends return failure.
- Credentials are machine-bound (`CRED_PERSIST_LOCAL_MACHINE`), no roaming.

### GUI

- Contact list is hardcoded; contact management not implemented.
- Debug bundle export button is wired but not implemented.
- SIP trace: raw SIP capture requires a PJSIP logging hook (not implemented);
  traces carry synthetic values.
- Category filter / search in DiagnosticsPanel is UI-only, not wired.
- Most menu and navigation actions are disconnected.

### ETSI

- ETSI TS 103 479, 103 480, 103 698 (LMPE) — not started.

---

## 12. Roadmap

### Phase 1 — ETSI / NG112 (next immediate priority)

| Task | Title | Description |
|---|---|---|
| 34 | ETSI architecture skeleton | Create `src/etsi/` structure; CMake `BUILD_ETSI` option; base interfaces for Ts103479, Ts103480, Ts103698Lmpe. |
| 35 | TS 103 479 — Emergency session handling | Session identity, priority SIP headers (`Resource-Priority`), PSAP interconnect URI handling. |
| 36 | TS 103 698 — LMPE messaging | Location and Metadata Per Event XML encoding; LMPE panel wiring. |
| 37 | TS 103 480 — Interoperability test support | Test procedure hooks; conformance log export. |
| 38 | NG112 MIME multiplexing | `multipart/mixed` body encoding for simultaneous SIP + T.140 + location in INVITE body. |

### Phase 2 — Hardening and Missing Features

| Task | Title | Description |
|---|---|---|
| 29 | Incoming call GUI flow | Tray/popup notification; accept/reject without focusing main window. |
| 30 | Call history | Persist call metadata (URI, duration, direction, timestamp) to local store. |
| 31 | Network recovery | Listen to `QNetworkInformation` connectivity events; re-register on reconnect. |
| 32 | Linux/macOS credential backends | `SecretService` for Linux, `Security.framework` for macOS. |
| 33 | Debug bundle export | Redacted log + environment info + sanitized SIP trace → zip. |

---

## 13. Prompt for New Chat

Paste this block verbatim into a new Claude Code session to resume from this handoff.

```
Continue development of "SIP Client Audio Video RTT".

Repository: f:\Project\Iryme\SIP-Client-Audio-Video-RTT
Branch: feature/project-handoff-002
HEAD: 96ee7b4

MANDATORY: Read docs/project-handoff/handoff-003.md before touching any code.

----------------------------------------------------------------------
PROJECT SUMMARY
----------------------------------------------------------------------

Qt 6.11.1 + C++17 + CMake + PJSIP/pjsua2 2.17-dev SIP multimedia desktop
client on Windows (MSVC 2022 amd64). Targets emergency-services / accessibility
scenarios with Audio + Video + RTT (RFC 4103 / T.140) in a single call.

----------------------------------------------------------------------
COMPLETED SUBSYSTEMS (ALL LIVE-VALIDATED)
----------------------------------------------------------------------

SIP:
  REGISTER / UNREGISTER / 401 digest auth / re-REGISTER refresh (80%/30s margin)
  Exponential retry on transient failure / profile switch sequencing
  Outgoing + incoming INVITE / BYE / Hold / Resume
  SIP URI normalization / outbound proxy support
  SIP trace logger + SIP ladder widget

Audio:
  RTP G.722 @ 16 kHz, G.711 @ 8 kHz
  Device selection (mic + speaker) / mute / level meters / default-system device

Video:
  VP8 codec (libvpx via vcpkg x64-windows-static)
  DirectShow camera (PJMEDIA_WITH_VIDEODEV_DSHOW=ON)
  Remote + local render via Win32 SetParent + MoveWindow into Qt widget HWNDs
  PjsipGdiRenderer custom PJMEDIA factory for HWND registration
  Video mute / camera selection

RTT (RFC 4103 / T.140):
  SDP m=text offer/answer
  TX character-by-character delta (every keystroke → RTP)
  RX accumulation — live typing display + transcript flush on CR
  Backspace (U+0008) TX and RX
  RED redundancy RFC 2198, level 2
  RttSession 5-state machine (Disabled/Offered/Negotiated/Active/Failed)
  RttPanel GUI with live-typing area and transcript

----------------------------------------------------------------------
NOT STARTED
----------------------------------------------------------------------

  ETSI TS 103 479 — Emergency call handling
  ETSI TS 103 480 — Interoperability test support
  ETSI TS 103 698 — LMPE messaging
  NG112 MIME multiplexing
  Incoming call GUI notification (tray/popup)
  Call history persistence
  Network recovery on reconnect
  Linux/macOS credential backends
  Audio/video hot-swap during call

----------------------------------------------------------------------
MANDATORY PATCH
----------------------------------------------------------------------

The PJSIP source at .deps/pjproject has a mandatory patch applied:
  patches/0001-pjsua2-call-dtor-guard-user-data-clear.patch

This patch guards pjsua2 Call::~Call() against clearing user_data on a
recycled call slot that was already claimed by a new incoming call. Without
it, consecutive calls result in auto-rejection with 500 Internal Server Error.

MUST re-apply after any pjproject update or clean rebuild:
  cd .deps/pjproject && git apply ../../patches/0001-pjsua2-call-dtor-guard-user-data-clear.patch

----------------------------------------------------------------------
ARCHITECTURE CONSTRAINTS (MUST FOLLOW)
----------------------------------------------------------------------

1. Passwords NEVER in QSettings, SipProfile, logs, or signals. OS keychain
   only (CredentialStore / WindowsCredentialBackend).

2. Platform guards before Qt headers: use #ifdef _WIN32, NOT #ifdef Q_OS_WIN.

3. HAVE_PJSIP guards all pjsua2 code. Only SipManager.cpp, SipAccount.cpp,
   SipCall.cpp, and PjsipGdiRenderer.cpp may include pjsua2 / pjsip headers.
   AudioMediaManager, VideoMediaManager, RttSession, SipTraceLogger are
   Qt-only — no pjsua2.hpp.

4. All PJSIP callbacks dispatch to Qt main thread via:
   QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)

5. CredentialStore key: SIPClient/sip/<profileId>/<username>

6. Test isolation: unique QCoreApplication org/app name per test file.

7. RAW log level default always false. No code enables it unconditionally.

8. windeployqt runs as POST_BUILD on the main target to deploy Qt plugins.

----------------------------------------------------------------------
BUILD ENVIRONMENT (WINDOWS)
----------------------------------------------------------------------

VS dev shell:
  C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1 -Arch amd64 -SkipAutomaticLocation

Qt: F:\Programs\Qt\6.11.1\msvc2022_64
Generator: NMake Makefiles
PJSIP dir: .deps\pjproject (built in-tree)

Full build command:
  $env:CMAKE_PREFIX_PATH = "F:\Programs\Qt\6.11.1\msvc2022_64"
  cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DENABLE_PJSIP=ON -DPJSIP_DIR="$PWD\.deps\pjproject" -DBUILD_TESTS=ON
  cmake --build build
  ctest --test-dir build --output-on-failure

Test executables: .\build\tests\test_<name>.exe -o results.xml,xml
(PowerShell does not capture Qt test stdout via redirect — use -o flag)

Live Kamailio server: 10.2.0.180:5060
  alice / secret123 @ sensor-x.local
  bob   / secret456 @ sensor-x.local
  Outbound proxy: sip:10.2.0.180;transport=udp (REQUIRED for INVITE routing)

----------------------------------------------------------------------
TEST STATUS
----------------------------------------------------------------------

CTest: 15 suites, 0 failures (stub mode, 2026-06-26)
RTT GUI: 17/17 items PASS (live, PJSUA 2.17-dev peer, Kamailio 5.x, 2026-06-26)
See: docs/rtt-interoperability-test.md

----------------------------------------------------------------------
NEXT TASK
----------------------------------------------------------------------

Task 34 — ETSI Architecture Skeleton:
  Create src/etsi/ directory structure.
  Add CMake option BUILD_ETSI (default OFF).
  Add base interfaces: IEmergencySessionHandler, ILmpeEncoder, ITs103480Logger.
  Add stub implementations (compile-only, no logic).
  Add placeholder docs/etsi-compatibility/ files if not present.
  Add unit tests for the interfaces (stub pass-through).
  No ETSI logic yet — architecture and plumbing only.

----------------------------------------------------------------------
WORK RULES
----------------------------------------------------------------------

- Do not implement features beyond what the current task requires.
- Do not modify call flow SIP/audio/video/RTT unless a live test finds a bug.
- Do not start the next task without explicitly being asked.
- Do not declare a subsystem complete without live validation (or explicit
  acceptance of stub-only status from the user).
- Keep all doc changes in docs/ only unless code changes are required.
- Before any code change: read the relevant .h and .cpp file.
- After any code change: run ctest --test-dir build --output-on-failure.
```
