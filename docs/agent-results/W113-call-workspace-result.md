# Agent Result — Task W113: Call Workspace

Full 39-item report per the W112–W117 roadmap's requirements. See
[docs/call-workspace.md](../call-workspace.md),
[docs/call-state-and-media-model.md](../call-state-and-media-model.md),
[docs/call-workspace-diagnostics.md](../call-workspace-diagnostics.md)
for the detailed architecture referenced throughout.

1. **Task**: W113 — Call Workspace.
2. **Branch**: `feature/w113-call-workspace`.
3. **Branch de pornire**: `feature/w112-conversation-workspace`.
4. **Commit de pornire**: the tip of W112 at the time this branch was
   created (W112's docs/final-report commit).
5. **Versiune veche**: `1.5.0`.
6. **Versiune nouă**: `1.6.0` (MINOR — new consolidated feature, no
   breaking change), matching the schedule W112 itself projected for W113.
7. **Application version**: `1.6.0`.
8. **Backend version**: same binary as frontend/UI — `1.6.0` (one binary
   ships both; no independent backend version invented).
9. **Frontend/UI version**: `1.6.0` (same reasoning as item 8).
10. **Schema version**: unchanged — `InteropTraceExporter::kSchemaVersion`
    still `3`; this task added no new export schema.
11. **Build type**: Debug and Release, both verified (see item 18).
12. **Git commit**: version-bump commit `36ef02e` precedes the feature
    work; see item 32 for the full commit list.
13. **Fișiere modificate**: 17 total — 1 in the version-bump commit
    (`CMakeLists.txt`); in the feature work: 4 new
    (`CallInfoModel.{h,cpp}`, `CallWorkspacePanel.{h,cpp}`), 1 new test
    (`test_call_info_model.cpp`), 3 new docs
    (`call-workspace.md`, `call-state-and-media-model.md`,
    `call-workspace-diagnostics.md`), 2 deleted (`CallPanel.{h,cpp}`), and
    modified: `CMakeLists.txt`, `tests/CMakeLists.txt`, `MainWindow.{h,cpp}`,
    `SipLadderPage.{h,cpp}`, `ToolsPage.{h,cpp}`, `ThemeManager.cpp`,
    `docs/release-notes.md`, `docs/project-status.md`.
14. **Arhitectură**: `CallWorkspacePanel` (QWidget, presentation + its own
    SipManager/VideoMediaManager/CameraController/PresenceStore/
    RttSession/EmergencyCallController signal connections, self-contained
    like the codebase's existing `ConversationWorkspacePanel` pattern) →
    `CallInfoModel` (dependency-free, fed via setters, same "fed
    externally" pattern as `ConversationListModel`/`CallHistoryListModel`)
    → status cards/buttons rendered from the model's state, never from
    button checked-state.
15. **Funcții implementate**: call header (remote identity resolved via
    `ContactStore`, presence, call state, duration), hold/resume (existing
    optimistic-UI + timeout-confirm pattern preserved), mute, camera
    on/off, video mute, request/accept video, request/accept RTT, media
    device status (mic/speaker/camera combos + level meters), jitter/loss/
    RTT stats (RTP-stats based, now correctly separated from video-drop
    count), selected/negotiated/actual media (three distinct, never-
    conflated concepts), SIP Ladder deep link filtered by Call-ID, single-
    call-launch entrypoint (`placeCall()`) used by all four call-initiation
    paths, emergency-call test-mode section (ported, previously
    unreachable).
16. **Bug-uri descoperite**: (a) `src/gui/panels/CallPanel` was a fully-
    built, never-instantiated widget — the only GUI entrypoint for
    emergency calling, unreachable even with `emergencyTestModeEnabled()`
    on; (b) a single status card (`cardPacketLoss`) was overwritten by two
    unrelated data sources (RTCP packet-loss % and video frame-drop count);
    (c) `ConversationWorkspacePanel::callRequested` and
    `CallHistoryPanel::redialRequested` both skipped URI normalization and
    always placed audio-only calls regardless of requested media, with
    failures never surfaced to the user.
17. **Bug-uri corectate**: all three — (a) emergency section folded into
    `CallWorkspacePanel` (still gated behind the same default-off setting),
    `CallPanel.{h,cpp}` deleted; (b) split into two cards
    (`m_cardPacketLoss` RTP %, `m_cardVideoDrops` raw count); (c) all four
    call-launch entrypoints (dialpad, conversation list, call history,
    contacts) now go through one `CallWorkspacePanel::placeCall()`
    (normalization + `CallMediaOptions` + status-bar feedback).
18. **Teste automate**: new `tests/test_call_info_model.cpp` (7 cases:
    defaults, setter/signal correctness, selected-vs-negotiated
    independence, packet-loss/video-drops separation, `reset()` clears
    every field, cross-call isolation). Debug and Release both rebuilt
    clean (0 errors/warnings) after every checkpoint (version bump,
    `CallInfoModel`, `CallWorkspacePanel` + wiring, `CallPanel` deletion),
    with a full CTest run after each.
19. **Teste manuale**: app launch/idle smoke — not re-run as a separate
    step this task (covered by the Debug/Release build+CTest checkpoints
    above); interactive click-through of the new workspace (place a call,
    toggle hold/mute/camera/video/RTT, open the SIP Ladder deep link, use
    the emergency test-mode button) is NOT RUN — see item 24.
20. **E2E**: N/A — the E2E laboratory is W114's deliverable; none exists
    yet in this repo.
21. **PASS**: Debug build, Release build, full CTest (80/80) on both
    configs, all new unit tests.
22. **FAIL**: none observed.
23. **BLOCKED**: none.
24. **NOT RUN**: interactive click-through of `CallWorkspacePanel` (place a
    call, hold/mute/camera/video/RTT toggles, SIP Ladder deep link,
    emergency test-mode button) — no live SIP peer or input-automation
    tooling was available in this session (a background coding session,
    not the live two-device workflow used for earlier crash-fix work in
    this repo's history).
25. **UNSUPPORTED**: none — everything NOT RUN is an environment
    constraint, not a missing/unsupported feature.
26. **Regresii**: zero — 80/80 CTest passed after every checkpoint in this
    task, Debug and Release.
27. **Limitări**: (a) "multiple call isolation" is rigorous sequential-call
    state reset (`CallInfoModel::reset()` on every Idle/Failed transition),
    not real concurrent-call support — `SipManager` remains single-active-
    call by design (`SipCall *m_activeCall`, confirmed no dead multi-call
    scaffolding), matching the same documented limitation from W111/W112;
    (b) `PresenceStore` keys by raw entity URI, not case-normalized (pre-
    existing, noted in W112, unchanged by this task); (c) a few QSS
    selectors in `ThemeManager.cpp` (`#AnswerBtn`, `#RejectBtn`,
    `#HangupBtn`, `#DialRow`, etc.) that only matched `CallPanel`'s
    specific object names are now orphaned/unused — harmless (styling
    still fully covered by the `#CallCtrlBtn[callRole=...]` selectors this
    task's buttons use) but not cleaned up, since removing unrelated dead
    CSS was judged out of scope for this task.
28. **Probleme client**: none found beyond items 16b/16c above (both
    fixed).
29. **Probleme server**: none — no `SipManager`/`SipCall`/PJSIP protocol
    logic changed in this task, only consumption of its existing public
    API.
30. **Probleme de mediu**: no input-automation/second-peer tooling
    available for the interactive manual pass (item 24); not a code
    defect.
31. **Documentație**: `docs/call-workspace.md`,
    `docs/call-state-and-media-model.md`,
    `docs/call-workspace-diagnostics.md` (all new); `docs/release-notes.md`
    (new `v1.6.0` entry), `docs/project-status.md` (W113 row + version
    line) — all updated.
32. **Commituri**: `36ef02e` (version bump, separate commit per the
    roadmap's rule) plus the feature/docs commits that follow it on
    `feature/w113-call-workspace`, off
    `feature/w112-conversation-workspace`.
33. **Git status**: clean at time of writing aside from this task's own
    changes and the two untracked diagnostics zip files pre-existing in
    the repo root from earlier live testing (untouched, not part of this
    task).
34. **Push status**: `git push -u origin feature/w113-call-workspace`
    executed after this report was written.
35. **Confirmare fără merge**: confirmed — no merge into `main`/`release`
    was performed.
36. **Confirmare privind pjproject**: confirmed — no file under any
    vendored PJSIP/pjproject directory was touched.
37. **Confirmare versiuni sincronizate**: Application/Backend/Frontend-UI
    all report `1.6.0` from the single `CMakeLists.txt` `project(VERSION)`
    source (via the generated `AppVersion.h`, wired in W112) — no
    divergence.
38. **Confirmare informații de versiune**: startup log, About dialog,
    Diagnostics Center panel, and diagnostics JSON/bundle export all
    already read from `AppVersion.h` (wired in W112) — this task made no
    further version-surface changes, since none were needed.
39. **Ce rămâne pentru W114**: the full Automated E2E Laboratory (two-
    Windows-machine Python controller + agents, `pytest` scenarios,
    stable automation IDs, packet capture, JUnit/summary-JSON artifacts).
    Also carries forward: a live two-device interactive manual pass of the
    Call Workspace (place/hold/mute/video/RTT/SIP-Ladder-link/emergency
    button); the orphaned QSS selectors noted in item 27c, if ever worth
    cleaning up; deciding whether `PresenceStore` should key on normalized
    URIs (carried from W112, still unresolved).
