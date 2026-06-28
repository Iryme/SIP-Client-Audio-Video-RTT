# Release Notes

See [versioning-and-rollout.md](versioning-and-rollout.md) for the versioning policy, rollout gate, and branch model.

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
