# Call Workspace (Task W113)

`CallWorkspacePanel` (`src/gui/panels/CallWorkspacePanel.{h,cpp}`) consolidates
every call control into one widget, replacing the ~1000 lines of ad hoc
lambdas previously inlined in `MainWindow::buildClientsPage()`. It occupies
the Clients page's center column, the same slot the old "Status / Device"
group box and call-control buttons used to fill; `VideoPanel` still sits
below it, unchanged.

## What it owns

- **Header/state**: call state badge, duration, remote URI (resolved to a
  saved contact name when one matches, via `ContactStore`), presence.
- **Controls**: call/answer/reject/hangup, mute, hold/resume (same
  optimistic-UI + timeout-confirm pattern as before), request video /
  request RTT (same accept-mode blink-timer behavior as before), camera
  on/off, video mute.
- **Media status grid**: audio/video/RTT state, negotiated audio/video
  codec, bitrate, resolution, FPS, selected media (what was requested when
  the call was placed), local account, device status, RTP jitter/latency,
  RTP packet loss (%), and local video frame drops — the last two are
  deliberately separate cards (see
  [call-state-and-media-model.md](call-state-and-media-model.md)).
- **Diagnostics**: an "Open in SIP Ladder" button that deep-links to
  Tools → SIP Ladder, pre-filtered to the active call's Call-ID.
- **Emergency test-mode section**: hidden unless
  `AppSettings::emergencyTestModeEnabled()` — see below.

## Single call-launch entrypoint: `placeCall()`

`CallWorkspacePanel::placeCall(const QString &uri)` normalizes the URI
(`SipUriNormalizer`, same fallback-domain logic as before), builds
`CallMediaOptions` from the current request-video/request-RTT button state,
and calls `SipManager::makeCall()`. Failures are surfaced via
`statusMessageRequested` (connected to `MainWindow`'s status bar) instead of
only being logged.

Every call-launch entrypoint now goes through this one method:

- The panel's own Call button (reads the shared dial-target `QLineEdit`,
  the same field the numeric dialpad and `ConversationWorkspacePanel` write
  to).
- `ConversationWorkspacePanel::callRequested`.
- `CallHistoryPanel::redialRequested`.
- `ContactsPanel::dialRequested`.

Before this task, `ConversationWorkspacePanel::callRequested` and
`CallHistoryPanel::redialRequested` both called the bare
`SipManager::makeCall(uri)` overload directly — skipping URI normalization
and always placing an audio-only call regardless of the dialpad's
video/RTT button state, with failures never surfaced to the user. This was
a real, if minor, inconsistency; consolidating onto `placeCall()` fixes it
for all four entrypoints at once.

## Supersedes the orphaned `CallPanel`

`src/gui/panels/CallPanel.{h,cpp}` was a fully-built, previously-compiled
call-control widget that was **never instantiated anywhere** in the app —
confirmed via a repo-wide grep before this task began. It was the only
place emergency-call UI existed, meaning even with
`emergencyTestModeEnabled()` turned on, there was no way to actually reach
emergency calling in the shipped app. This task ports that section
(unchanged behavior: confirmation dialogs, manual PIDF-LO generation,
in-dialog location update) into `CallWorkspacePanel`, then deletes
`CallPanel.{h,cpp}` — it added nothing `CallWorkspacePanel` doesn't already
cover.

## Multiple-call isolation

`SipManager` is single-active-call by design (`SipCall *m_activeCall`,
confirmed no per-call-id map or dead multi-call scaffolding anywhere).
"Isolation" in this task means: every field in `CallInfoModel` (see
[call-state-and-media-model.md](call-state-and-media-model.md)) is reset on
every transition to Idle/Failed, so nothing from a finished call can leak
into the next one's display. Real concurrent multi-call support would
require an architectural change to `SipManager`/`SipCall` and is out of
scope for this GUI-consolidation task — carried forward as a documented
limitation, the same pattern already used for this in W111/W112.
