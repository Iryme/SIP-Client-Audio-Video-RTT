# Call Workspace Diagnostics (Task W113)

## SIP Ladder deep link

`CallWorkspacePanel` has an "Open in SIP Ladder" button that emits
`openSipLadderRequested(const QString &callId)` with
`SipManager::activeCallSipId()`. `MainWindow` connects this to:

1. `onNavPageRequested(QStringLiteral("sipladder"))` — switches to Tools
   and ensures the SIP Ladder sub-tab exists (existing deep-link mechanism,
   already used by `DiagnosticsCenterPanel`).
2. `ToolsPage::filterSipLadderByCallId(callId)` (new) — ensures the SIP
   Ladder sub-tab is built, then calls the new
   `SipLadderPage::filterByCallId(callId)` (new), which sets the Call-ID
   filter field and re-applies filters immediately.

This means clicking the button while a call is active (or just ended)
lands on Tools → SIP Ladder pre-filtered to that call's messages, instead
of requiring the user to manually copy the Call-ID and paste it into the
filter field.

## RTP stats guard reuse

See [call-state-and-media-model.md](call-state-and-media-model.md) — no
new direct pjsua2 `getInfo()` call site was added by this task; the
existing `SipCall::mediaRtpStats()` guard (`isTeardownState()` + try/catch)
is reused via `SipManager::currentRtpStats()`/`rtpStatsChanged`.

## Emergency-call section gating

The emergency test-mode section (ported from the previously-orphaned
`CallPanel` — see [call-workspace.md](call-workspace.md)) stays hidden
unless `AppSettings::emergencyTestModeEnabled()` returns true, unchanged
from `CallPanel`'s original gating. This is a "test/lab only" feature that
sends real SIP INVITEs to a configured lab PSAP target with NG112 headers
and a manually-entered (not GPS-derived) PIDF-LO location — never enabled
by default, matching the roadmap's "experimental features stay disabled by
default" rule. Since `CallPanel` itself was never instantiated before this
task, this section was previously **completely unreachable** regardless of
the setting; this task is what makes the existing, already-tested
`EmergencyCallController`/`EmergencyCallAdapter`/`EmergencyInviteBuilder`
logic (covered by `tests/test_emergency_gui_wiring.cpp` and friends) usable
from the running application for the first time.
