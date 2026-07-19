# Release Notes

## macOS v1.6.9 — Camera Off Video Stream Guard

**Platform version:** macOS 1.6.8 → 1.6.9 (build 169). Windows remains 1.6.8.

- Fixes the macOS SIGABRT path
  `CameraController → SipManager → SipCall → Call::vidSetStream →
  pjmedia_vid_stream_pause` when Camera Off arrived without a valid PJSIP
  video stream.
- STOP/START_TRANSMIT now requires a live call, non-teardown state, valid call
  ID, matching video media/index, ACTIVE status, and transmit direction.
- Missing codec/media, audio-only, inactive/error, post-BYE, teardown, and
  repeated actions are safe no-ops at the PJSIP boundary. Local Qt preview is
  still stopped independently, and Camera On does not imply active call video.
- `pj::Error` diagnostics include status, title, reason, call state, and media
  index. No pjproject sources or codecs changed.
- The guard is common code. Windows rebuild/live regression is required in a
  separate task; Windows is not declared updated by this macOS release.

See [versioning-and-rollout.md](versioning-and-rollout.md) for the versioning policy, rollout gate, and branch model.

---

## v1.6.8 — Native macOS ARM64 Bundle

**Status:** complete for native build/package automation; external
interoperability gates remain open
**Branch:** `feature/w113g-macos-arm64-bundle`
**Version bump type:** PATCH (1.6.7 → 1.6.8 as required by W113G)
**Scope:** Apple Silicon build and deployment support. No pjproject source
changes, no LMPE activation, and no deliberate Windows call-flow change.

- Added a native macOS `SIP Client.app` with minimum macOS 13.0, bundle
  identifier, version/build metadata, and camera/microphone usage strings.
- Isolated Windows GDI and Credential Manager sources; added a real macOS
  Keychain backend through Apple's Security framework.
- Extended PJSIP discovery for official GNU-install, target-suffixed static
  archives and macOS system frameworks. The validated PJSIP 2.17 build uses
  CoreAudio, AVFoundation, Metal, VideoToolbox, libyuv, WebRTC AEC, and static
  OpenSSL without editing upstream sources.
- Added `scripts/package-macos.sh`: native ARM64 configure/build/test,
  `macdeployqt`, slice thinning, Mach-O/dependency/RPATH/security audits,
  nested-first signing, relocated smoke test, ZIP/SHA/manifest, optional DMG,
  Developer ID and notarization workflow.
- Native Debug and Release builds PASS; 82/82 tests PASS; deployed app
  architecture/audit/ad-hoc signature/smoke/relocation checks PASS.
- Developer ID signing and notarization: **NOT RUN / BLOCKED** (no identity or
  keychain profile). Second clean Apple Silicon Mac: **NOT RUN / BLOCKED**.
  Windows build and Windows↔macOS live REGISTER/audio/video/RTT/messaging:
  **NOT RUN / BLOCKED** (no Windows host, SIP server/accounts, or peer).
- PJSIP's validated private build had no VPX/OpenH264/FFmpeg video codec, so
  native capture/render backends compiled but a real video call is not
  certified. See [macos-audio-video.md](macos-audio-video.md).

Full evidence and artifact metadata: [W113G result](agent-results/W113G-macos-arm64-bundle-result.md).

## v1.6.7 — Simplify Client Messaging, Hide Protocol XML and Force LMPE Inactive

**Status:** complete
**Branch:** `fix/w113f-simplify-client-messaging-and-disable-lmpe`
**Version bump type:** PATCH (task spec assumed a starting version of
1.6.5 and a new version of 1.6.6; the real starting version was already
1.6.6 (W113E) at the time this task began, so this bumps 1.6.6 → 1.6.7 —
documented substitution, see the agent-result report)
**Scope:** Messaging pipeline correctness fix + Client UI simplification +
LMPE hard-disable. No changes to RTT/video negotiation, no new protocols,
no pjproject changes.

### Root cause: protocol XML in the conversation

Two real bugs, both now fixed via one shared routing function
(`SipManager::routeInboundMessagingPayload()`):

- Plain SIP MESSAGE had no `message/cpim` branch — a CPIM-wrapped message
  rendered as a chat bubble containing the raw envelope text.
- MSRP's inbound path never classified anything at all. A separate class,
  `MsrpPayloadDispatcher`, already modeled the correct unwrap-and-classify
  logic and had its own passing tests — but nothing in the production
  receive path ever called it (confirmed by a full-repo grep). Any CPIM/
  IMDN/is-composing payload carried over MSRP rendered as a plain message
  too.

Both paths now share one function: CPIM is unwrapped first (inner
Content-Type re-classified, so a CPIM-wrapped IMDN/is-composing
notification is caught), then IMDN/is-composing are classified and routed
to their own history rows exactly like the plain-SIP-MESSAGE path already
did for unwrapped payloads. A CPIM envelope that fails to parse becomes a
safe "Unsupported or malformed message" placeholder
(`MessageHistoryStore::appendInboundUnsupported()`) — the raw envelope
text is never stored or rendered.

### Client Messaging UI: protocol events excluded, view simplified

- New `MessageHistoryEntry::isProtocolEvent()` (true for IMDN reports,
  is-composing notifications, unsupported/malformed payloads) and
  `ConversationModel::userVisibleHistoryFor()` (the same history filtered
  to exclude them). `ClientMessagingView`'s history list, and
  `ConversationModel::lastMessageFor()`/`unreadCountFor()` (conversation
  preview text / unread badge, read by `ConversationWorkspacePanel`), all
  now use this — an IMDN report or typing notification can never appear as
  a chat bubble, become a conversation's preview text, or count toward its
  unread badge. Tools' Message History table intentionally still shows
  every row, protocol events included.
- `ClientMessagingView`'s transport selector, content-type selector, and
  delivery-receipt checkboxes moved behind a collapsed-by-default
  "Messaging options" disclosure (same persistence pattern as
  `CallWorkspacePanel`'s "Advanced diagnostics" from Task W113a).
- CPIM removed as a manually selectable content type — it was never what
  actually controlled CPIM wrapping anyway (a separate
  `AppSettings::enableCpim()` global toggle already does that); exposing
  it as a per-message choice only invited confusion. Only Plain text and
  HTML remain as content-type options.

### LMPE: permanently unavailable everywhere

LMPE has no interoperable wire format (`UnconfirmedLmpeCodec` always
returns a blocked result) — was already fully inert at the call/media
level, but the UI didn't show that:

- `SipProfileEditorDialog`'s "Enable LMPE" checkbox: disabled, relabeled
  "Enable LMPE — unavailable", tooltip explaining why, force-unchecked
  regardless of a saved profile's value.
- `RttPanel`'s LMPE tab was previously a fully interactive panel with its
  own input box, Send button, and a local-echo list — none of which ever
  sent anything anywhere. Replaced with a single disabled label,
  "LMPE — unavailable", with the same tooltip.
- `CallWorkspacePanel`'s LMPE status card: `"—"` (ambiguous) →
  `"Unavailable"` (explicit), always.
- `SipProfileManager::loadAllProfiles()` forces `enableLmpe=false` at the
  single authoritative load point, regardless of what's saved — an old
  profile or hand-edited/imported config with `enableLmpe: true` logs one
  redacted warning (profile id only) and loads disabled, never crashes.

### Documentation corrections

`docs/imdn.md`, `docs/is-composing.md`, and `docs/msrp-foundation.md`
previously claimed MSRP-carried IMDN/is-composing reused
`MsrpPayloadDispatcher` — corrected to describe the actual production path
(`SipManager::routeInboundMessagingPayload()`), with a note that
`MsrpPayloadDispatcher` itself is untouched, still tested, but not what's
actually wired into the live receive path.

### Validation

- Debug (`build/`) rebuilt clean, 81/81 CTest (80 baseline + 15 new test
  cases: `test_message_history` gained placeholder/dedup/`isProtocolEvent()`
  coverage, `test_conversation_model` gained `userVisibleHistoryFor()`/
  preview/unread-count exclusion coverage, `test_sip_profile_manager`
  gained an LMPE-forced-disabled-on-reload test).
- Release (`build-release/`) rebuilt clean, 81/81 CTest.
- Manual two-peer GUI test (Alice/Bob exchanging plain text, typing, IMDN,
  Presence, CPIM, explicit SIP MESSAGE/MSRP transport, fallback, LMPE
  unavailable confirmation, restart) — **NOT RUN**: no live SIP peer or
  second client instance available this session. Not declared PASS.

---

## v1.6.6 — RTT Request Visual Alert Parity with Video

**Status:** complete
**Branch:** `fix/w113e-rtt-request-visual-alert`
**Version bump type:** PATCH
**Scope:** UI/state-binding only. An incoming RTT request now flashes the
Request RTT button exactly like an incoming video request flashes Request
Video — same color, same 500ms interval, same start/stop conditions. No
changes to RTT/video SDP negotiation, `requestRtt()`,
`acceptIncomingRttRequest()`, `rejectIncomingRttRequest()`, RTP port
allocation, or either state machine.

### What changed

- New `src/gui/widgets/RequestBlinker.{h,cpp}` — a small, reusable
  flashing-indicator helper (500ms `QTimer` + bool, `start()`/`stop()`/
  `isOn()`, both idempotent). Video's previously one-off
  `m_videoRequestBlinkTimer`/`m_videoRequestBlinkOn` pair was replaced with
  an instance of this class; RTT gets its own instance of the same class.
  One shared implementation, not two that could drift apart.
- `CallWorkspacePanel` now starts/stops `m_rttRequestBlinker` at every
  point video already starts/stops its own blinker (request received,
  accept, call disconnected/failed, media connected/disconnected, panel
  reset), plus three RTT-only stop signals that have no video equivalent
  (`rttRequestRejected`, `rttRequestWithdrawn`, `rttNegotiationFailed` —
  the underlying `SipCall`/`SipManager` layer only exposes these for RTT).
- `ThemeManager.cpp` — new shared `[rttAlert="true"]` QSS rule, identical
  color to `[videoAlert="true"]`. Also fixed two style gaps found while
  centralizing this: `callRole="acceptRtt"` previously had no QSS rule at
  all (fell back to the unstyled platform default), and
  `callRole="rttActive"` had no distinct "active" look unlike
  `callRole="videoActive"`'s green style — both now share their video
  counterpart's rule.
- Both Request Video and Request RTT buttons now have a `toolTip()`,
  `accessibleName()`, and `accessibleDescription()` that update with
  state — neither had any accessibility metadata before, and the alert
  was effectively color-only until now.

### Validation

- Debug (`build/`) rebuilt clean, 81/81 CTest (80 baseline + new
  `test_request_blinker`, covering `RequestBlinker`'s default state,
  start/stop idempotency, immediate-emit-on-start, and real timer-tick
  toggling).
- Release (`build-release/`) rebuilt clean, 81/81 CTest.
- Manual GUI click-through of the two-peer incoming-request scenario (Bob
  sends a video request, then an RTT request, confirm identical flashing,
  accept/reject/timeout, multiple-calls isolation) is **NOT RUN** — no
  live SIP peer or input-automation tooling available in this session,
  same constraint flagged on every prior GUI-facing task.

---

## v1.6.5 — Video Latency & Framerate Investigation

**Status:** complete
**Branch:** `fix/video-latency-and-codec-investigation`
**Version bump type:** PATCH
**Scope:** Investigation requested ahead of W113E, after a manual Alice/Bob
test session reported video/framerate problems and high latency on a
local network. Two small, low-risk UX/logging fixes landed; the rest of
the investigation's findings are documented for follow-up, not code
changes, per what they actually require (see
[video-latency-and-framerate-investigation.md](video-latency-and-framerate-investigation.md)
for the full writeup).

### What's fixed

- `CodecManager::applyVideoCodecOrder()` now logs a clear warning when a
  user's #1 preferred video codec has no matching entry in this PJSIP
  build's `videoCodecEnum2()` (this build only has VP8 compiled in — no
  H264/H265/AV1/VP9 — while the default preference order lists H264
  first), instead of silently falling back with no trace of why.
- `VideoSettingsPanel`'s codec reorder list now visually marks (grayed
  text + tooltip) any codec name not backed by a real codec on the
  current PJSIP build, so reordering an unavailable entry doesn't look
  like it did something it can't. Also fixed the list's save path to read
  the plain codec name from item data rather than the annotated display
  text, so the new UI marker can't corrupt a saved `codecOrder`.

### What's confirmed but NOT fixed (follow-up needed)

- **No live in-call video telemetry exists.** The Call Workspace's
  FPS/Video-Drops status cards and the diagnostics export's `videoFps`/
  `videoBitrateKbps` all trace back to `VideoStatistics`, which is only
  fed by the idle (out-of-call) Qt camera-preview loop —
  `VideoPanel::onVideoMediaConnected()` stops that loop for the entire
  duration of every real call. There is currently no way, from inside the
  app, to see whether a call is actually hitting its configured fps or
  badly under-delivering. Recommended follow-up: poll PJSIP's real video
  stream stats (`pjmedia_vid_stream_get_stat()`) while a call's video is
  active.
- This PJSIP build has exactly one usable video codec, VP8, software-only
  (no hardware H264 path). Likely a real contributor to perceived latency
  under load, but adding H264/OpenH264 support is a MAJOR-scope build
  change, not something to do inline here.
- One side of the test session used an RDP-redirected camera, adding
  latency inherent to that test setup, unrelated to the app's own code.
- Repeated video-window re-attach log lines and one RTT negotiation
  timeout observed in the test logs were both traced to expected behavior
  (manual Camera On/Off toggling during the test; the RTT offer's
  designed 12s timeout guard, respectively) — not defects.

### Validation

- Debug (`build/`) rebuilt clean, 80/80 CTest.
- Release (`build-release/`) rebuilt clean, 80/80 CTest.
- No new automated tests added: the codec-order warning only exercises
  inside `#ifdef HAVE_PJSIP`, which the existing `test_codec_manager`
  suite explicitly skips (`QSKIP` when `HAVE_PJSIP` is defined — it only
  covers the stub-mode codec list); the `VideoSettingsPanel` UI change has
  no existing widget-level test harness to extend. Verified by reading
  through both code paths and by the two full Debug/Release rebuilds
  above.

---

## v1.6.4 — Portable Windows Test Bundle

**Status:** complete
**Branch:** `release/w113d-portable-windows-bundle`
**Version bump type:** PATCH
**Scope:** New distribution artifact — a self-contained, portable Windows
x64 Release bundle for testing SIPClient on a machine with no Qt, Visual
Studio, or PJSIP dev environment installed. No product-feature changes.

### What's new

- `cmake/AppVersion.rc.in` — a real Windows `FILEVERSION`/`PRODUCTVERSION`
  resource, generated from the same `PROJECT_VERSION` CMake already uses
  for the in-app version strings. Previously `SIPClient.exe` carried no
  Windows version metadata at all (Explorer's Properties > Details tab had
  nothing to show); now it reports `1.6.4.0`, matching the About dialog,
  startup log, and Diagnostics export.
- `scripts/package-windows.ps1` — reproducible packaging script
  (parameters: `BuildDir`, `Configuration`, `QtBinDir`, `OutputDir`,
  `Version`, `IncludeSymbols`, `IncludeVcRedist`, `Clean`, `Archive`) that:
  stages `SIPClient.exe` + a generated `README-PORTABLE.txt` +
  `version-info.json`; runs `windeployqt --compiler-runtime
  --no-translations`; copies the VC++ runtime DLLs directly from the
  toolchain's own redist folder (`%VCToolsRedistDir%`) — `windeployqt
  --compiler-runtime` turned out to silently deploy zero CRT DLLs on this
  machine's VC 14.51 toolset, caught by the dependency audit rather than
  assumed to have worked; audits every staged DLL with `dumpbin
  /dependents` against a Windows-system-DLL allow-list (PJSIP, OpenSSL,
  zlib, and vpx are statically linked into `SIPClient.exe` — there is no
  runtime DLL for any of them to ship); scans staging for
  secrets/credentials/local paths; verifies x64-only and no Debug-suffixed
  DLLs; smoke-tests the **staged** exe (never the build-tree copy) under a
  `PATH` reduced to `%SystemRoot%\System32` so it can't accidentally
  resolve a build-machine Qt/VS DLL; and produces the zip, its SHA-256, a
  per-file manifest JSON, and an optional separate symbols zip. Fails with
  a non-zero exit on any critical problem — no step is masked with
  continue-on-error.
- `docs/windows-portable-bundle.md`, `docs/windows-deployment-dependencies.md`,
  `docs/windows-clean-machine-test.md` — the packaging procedure, the full
  dependency audit rationale, and the manual clean-machine test checklist.

### Artifact

`SIP-Client-Audio-Video-RTT-1.6.4-windows-x64-portable.zip` (43 files, one
top-level folder, no `.pdb`/source/build-cache/test binaries/personal
config), SHA-256 `78e55734f5e12d50287f97f6a48baa075356947a784e3ecedaf46618d548003c`,
plus `-manifest.json` and a separate `-symbols.zip`.

### Validation

- Debug (`build/`) rebuilt clean, 80/80 CTest.
- A from-scratch, freshly-configured Release x64 tree
  (`build-windows-x64-release/`, `ENABLE_PJSIP=ON`) built clean, 80/80
  CTest.
- Packaging script: dependency audit PASS (no unresolved non-system DLL),
  secrets/local-path scan PASS, x64/no-Debug-DLL check PASS, staging smoke
  test PASS (process starts and closes cleanly under a minimal `PATH`).
- `version-info.json` and the staged exe's own Windows version resource
  both report `1.6.4` / commit `4d05a04` / Qt `6.11.1` / PJSIP `2.17.0`
  consistently.
- **Clean-machine test (second VM/PC/Windows Sandbox): NOT RUN** — no
  clean Windows machine was available in this session. The in-session
  smoke test only proves the dependency closure is complete on *this*
  machine with a minimized `PATH`; it does not substitute for verifying
  REGISTER, audio/video calls, RTT, camera LED behavior, messaging, and
  config persistence on a machine that never had Qt/VS/PJSIP installed.
  See [windows-clean-machine-test.md](windows-clean-machine-test.md) for
  the checklist to run before treating this bundle as fully field-validated.

---

## v1.6.3 — Camera LED Stays On After Camera Off

**Status:** complete
**Branch:** `fix/w113c-camera-led-stays-on`
**Version bump type:** PATCH
**Scope:** Bug fix reported directly by the user: after clicking "Camera
Off" (outside an active call), the physical camera's hardware LED stayed
lit even though the app correctly showed the camera as disabled.

### Root cause and fix

`CameraController` is a pure flag+signal bus — it holds no camera handle
itself; every subscriber must actually release its own capture device.
`VideoPanel`'s idle preview (`m_clientsVideoPanel`, auto-started at app
init, independent of any call) is one such subscriber:
`VideoPanel::stopIdlePreview()` called `m_previewCamera->stop()` then
immediately `delete m_previewCamera` while it was still wired into
`m_previewSession` (`m_previewSession->setCamera(m_previewCamera)` was
never cleared). Qt Multimedia's Windows Media Foundation backend tears
down the device topology asynchronously after `stop()`; deleting the
`QCamera` object while still attached to a live capture session can leave
that teardown incomplete, so the physical device stays open at the driver
level (LED lit) even though the app's own state is correctly "off". This
is the same class of bug already fixed for the in-call PJSIP capture path
(`SipCall::pauseCapture()`/`resumeCapture()`, commit `9326d0f`) — this
task applies the analogous fix to the idle Qt-side preview.
`stopIdlePreview()` now detaches the camera from the session
(`setVideoSink(nullptr)`, `setCamera(nullptr)`) before stopping/deleting
it. Also updated three stale comments left over from the deleted
`CallPanel` class (renamed to `CallWorkspacePanel` in W113) that referred
to it by its old name.

### Validation

- Debug + Release rebuilt clean. Full CTest: 80/80 (unchanged — no tested
  logic touched; `QCamera`/hardware teardown isn't unit-testable without a
  real capture device).
- Manual interactive verification (click Camera Off while idle, confirm
  the physical LED turns off) NOT RUN in this session — no camera hardware
  or interactive session available; the fix follows directly from the
  documented Qt Multimedia teardown-ordering issue and mirrors the
  already-proven fix pattern used for the in-call path.

---

## v1.6.2 — Tools Tab-Switching Fix

**Status:** complete
**Branch:** `fix/w113b-tools-tab-switch-bug`
**Version bump type:** PATCH
**Scope:** Bug fix reported directly by the user: clicking a not-yet-built
Tools sub-tab showed its "Loading …" placeholder and then selection
visibly jumped to the *next* tab instead of the one clicked.

### Root cause and fix

`ToolsPage::ensureSubTab()` lazily replaces a tab's placeholder with the
real page via `removeTab(index)` followed by `insertTab(index, real, label)`.
When called from the `QTabWidget::currentChanged` handler (the normal path
for a user clicking a tab), `removeTab()` on the tab that was just made
current shifts `QTabWidget`'s current index to a neighboring tab —
`insertTab()` does not restore selection on its own. `ensureSubTab()` now
calls `m_tabs->setCurrentIndex(index)` after re-inserting the real page,
so the clicked tab stays selected.

### Validation

- Debug + Release rebuilt clean. Full CTest: 80/80 (unchanged — no tested
  logic touched).
- Manual interactive verification (click a not-yet-visited Tools sub-tab)
  NOT RUN in this session — no input-automation tooling available; the fix
  follows directly from the `QTabWidget`/`removeTab`/`insertTab` mechanics
  described above, not from reproducing the visible symptom in this
  session.

---

## v1.6.1 — Clients Page Layout Reorganization

**Status:** complete
**Branch:** `fix/w113a-client-workspace-layout`
**Version bump type:** PATCH
**Scope:** Usability follow-up requested directly after W113, before
continuing the roadmap to W114 — the Clients page's 4-column layout was
too wide and columns 1/2 too internally dense. See
[clients-page-layout.md](../clients-page-layout.md) for the full
before/after breakdown.

### Changes

- Clients page reduced from 4 to 3 splitter columns: Conversations/
  Contacts (tabbed, dialpad removed), Call Workspace + Video (unchanged
  position), Messaging/RTT (tabbed).
- `CallWorkspacePanel`'s 21 status cards split into an always-visible
  essential row (8 cards) and a collapsed-by-default "Advanced
  diagnostics" disclosure (13 cards), persisted via a new
  `AppSettings::callWorkspaceAdvancedDiagnosticsExpanded()` key.
- `QSplitter::restoreState()` now guarded against the widget-count
  mismatch a 4→3-column upgrade produces, falling back to new default
  sizes instead of a broken/unproportioned restore.

### Validation

- Debug + Release rebuilt clean. Full CTest: 80/80 (unchanged — this task
  touches only widget layout/grouping, no tested logic).
- Manual GUI smoke: app launch/idle reachable, no crash. Full interactive
  click-through (tab switching, expand/collapse advanced diagnostics) NOT
  RUN in this session — no input-automation tooling available.

---

## v1.6.0 — Call Workspace

**Status:** complete
**Branch:** `feature/w113-call-workspace`
**Version bump type:** MINOR
**Scope:** Second task of the W112–W117 roadmap. Consolidates every call
control (header, identity, presence, duration, hold/mute/camera/video/RTT,
media status grid, device status, jitter/loss/RTT stats, selected/
negotiated media, a SIP Ladder deep link, and the emergency-call test-mode
section) into one widget, `CallWorkspacePanel`, replacing ~1000 lines of ad
hoc lambdas previously inlined in `MainWindow::buildClientsPage()`.

### Features

**`CallWorkspacePanel` (`src/gui/panels/CallWorkspacePanel.*`, new)**
- Single source of truth for call display state via a new `CallInfoModel`
  (`src/gui/panels/call/CallInfoModel.*`) — fed via setters, never inferred
  from a button's checked-state.
- Selected (what was requested at call-launch), negotiated (what the SDP
  exchange produced), and actual (live RTP stats + video FPS/drops) media
  are tracked as three distinct concepts, never conflated.
- "Open in SIP Ladder" deep-links to Tools → SIP Ladder, pre-filtered to
  the active call's Call-ID (new `SipLadderPage::filterByCallId()` /
  `ToolsPage::filterSipLadderByCallId()`).
- Supersedes the previously-orphaned `src/gui/panels/CallPanel` (compiled
  but never instantiated anywhere) — folds in its emergency-call test-mode
  section (the only GUI entrypoint for emergency calling, unreachable
  before this task), audio-codec card, and initial-offer/selected-media
  card. `CallPanel.{h,cpp}` is deleted.

### Bug fixes

- **Packet-loss / video-drop conflation**: a single status card was
  previously overwritten by two unrelated data sources (RTCP packet-loss
  percentage and the local video pipeline's frame-drop count). Now two
  separate cards.
- **`callRequested`/`redialRequested` skipped normalization**: calls placed
  from the conversation list or call history previously bypassed URI
  normalization and always placed audio-only calls, with failures never
  surfaced to the user. All four call-launch entrypoints (dialpad,
  conversation list, call history, contacts) now go through one
  `CallWorkspacePanel::placeCall()`.

### Limitations

- "Multiple call isolation" means rigorous state reset between sequential
  calls (`CallInfoModel::reset()` on every Idle/Failed transition) —
  `SipManager` remains single-active-call by design; true concurrent calls
  are out of scope, matching the same documented limitation from W111/W112.

### Validation

- Debug + Release rebuilt clean after every step.
- Full CTest: 80/80 passed on both configs (79 baseline from W112 + 1 new
  `test_call_info_model` suite).
- Manual GUI smoke: app launch/idle reachable, no crash. Interactive
  click-through (place a call, toggle hold/mute/video/RTT, SIP Ladder deep
  link, emergency test-mode button) is NOT RUN in this session — no live
  peer or input-automation tooling available.

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
