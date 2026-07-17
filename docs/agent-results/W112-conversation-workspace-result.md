# Agent Result — Task W112: Conversation Workspace

Full 39-item report per the W112–W117 roadmap's requirements. See
[docs/conversation-workspace.md](../conversation-workspace.md),
[docs/conversation-model.md](../conversation-model.md),
[docs/conversation-state-and-correlation.md](../conversation-state-and-correlation.md)
for the detailed architecture referenced throughout.

1. **Task**: W112 — Conversation Workspace.
2. **Branch**: `feature/w112-conversation-workspace`.
3. **Branch de pornire**: `feature/w111-client-messaging-and-tools-navigation`.
4. **Commit de pornire**: `b73408d` ("docs(w111): document Tools navigation,
   Client Messaging View, and final report").
5. **Versiune veche**: `1.4.1` (real baseline — git tag + release-notes.md,
   *not* `CMakeLists.txt`'s stale `0.1.0`; see item 6 for the reconciliation).
6. **Versiune nouă**: `1.5.0` (MINOR). The roadmap's own suggested schedule
   assumed a `0.1.0` starting point; this task's audit found
   `CMakeLists.txt`'s `project(VERSION 0.1.0)` and two hardcoded `"0.1.0"`
   literals in `Application.cpp` were simply never updated, disconnected
   from the real, already-tagged release history (`v1.2.0`…`v1.4.1`). Per
   the roadmap's own override clause, this task bumps from the real
   baseline instead: `1.4.1` → `1.5.0`. Projected schedule for the rest of
   the roadmap under this corrected baseline (to be re-confirmed at the
   start of each task): W113 → 1.6.0, W114 → 1.7.0, W115 → 1.8.0 or a PATCH
   depending on actual scope, W116 → 1.9.0, W117 → 1.10.0-rc.1 (not the
   roadmap's literal `1.0.0-rc.1`, which would be a MAJOR *downgrade* from
   the real current version — nonsensical under SemVer ordering).
7. **Application version**: `1.5.0`.
8. **Backend version**: same binary as frontend/UI — `1.5.0` (this app ships
   backend+UI from one binary; no separate backend version exists or is
   invented, per the roadmap's own "don't invent independent versions when
   there's one binary" rule).
9. **Frontend/UI version**: `1.5.0` (same reasoning as item 8).
10. **Schema version**: unchanged — `InteropTraceExporter::kSchemaVersion`
    still `3`; this task added no new export schema.
11. **Build type**: Debug and Release, both verified (see item 18).
12. **Git commit**: `4ee9256` (feature commit; version-bump commit `f901106`
    precedes it — see item 32).
13. **Fișiere modificate**: 18 total — 2 in the version-bump commit
    (`CMakeLists.txt`, `src/app/Application.cpp`), 16 in the feature commit
    (7 new: `ConversationWorkspacePanel.{h,cpp}`,
    `ConversationListModel.{h,cpp}`, `ConversationFilterProxyModel.{h,cpp}`,
    `test_conversation_list_model.cpp`; 9 modified:
    `ConversationModel.{h,cpp}`, `ClientMessagingView.{h,cpp}`,
    `MainWindow.{h,cpp}`, `AppSettings.h`, `tests/CMakeLists.txt`,
    `test_conversation_model.cpp`).
14. **Arhitectură**: `ConversationWorkspacePanel` (QWidget, presentation) →
    `ConversationListModel`/`ConversationFilterProxyModel` (dependency-free,
    fed via `setRows()`, same pattern as `CallHistoryListModel`) →
    row data derived from `ContactStore` ∪ `ConversationModel` (single
    shared instance, exposed via a new `ClientMessagingView::controller()`
    accessor) ∪ `PresenceStore` ∪ `SipManager`. No protocol logic added to
    any widget.
15. **Funcții implementate**: conversation list (search + pinned/last-
    activity sort), last message + timestamp, unread count, presence,
    remote typing, actual transport, call state (single-active-call
    scoped), pin/favorite (new, persisted), call action started from a
    conversation, full per-conversation isolation (history/typing/unread).
    File transfer state column exists (`ConversationRow` has no dedicated
    field — reused `lastMessagePreview`/`actualTransportText` since W111's
    file-transfer flow already appends into `MessageHistoryStore`; a
    dedicated file-transfer-state column is deferred, see item 27/38).
16. **Bug-uri descoperite**: (a) the CMake/Application.cpp version drift
    (item 6); (b) a genuine infinite-recursion risk in the new
    `markRead()`/`conversationUpdated` interaction (a view marking its own
    currently-open conversation read on every update would re-trigger
    itself indefinitely).
17. **Bug-uri corectate**: both of the above — (a) fixed via the version
    bump + `AppVersion.h` wiring; (b) fixed by making `markRead()` a no-op
    (skip the emit) when the read cursor doesn't actually advance.
18. **Teste automate**: `test_conversation_model.cpp` extended (+8 cases:
    `lastMessageFor`/`lastActivityFor`/`unreadCountFor`/`markRead`
    correctness and cross-conversation isolation); new
    `test_conversation_list_model.cpp` (6 cases: row/role correctness, text
    filter, pinned-first + last-activity sort). Both Debug and Release
    rebuilt clean (0 errors/warnings) and full CTest run after every step.
19. **Teste manuale**: app launch/idle smoke PASS (Dashboard reachable, no
    crash) both immediately after this task's changes. Interactive
    click-through of the new workspace (select a conversation, pin/unpin,
    call from a conversation, search) is NOT RUN — see item 24.
20. **E2E**: N/A — the E2E laboratory is W114's deliverable; none exists
    yet in this repo.
21. **PASS**: Debug build, Release build, full CTest (79/79) on both
    configs, app launch/idle smoke, all new unit tests.
22. **FAIL**: none observed.
23. **BLOCKED**: none.
24. **NOT RUN**: interactive click-through of `ConversationWorkspacePanel`
    (search, select, pin, call-from-conversation) — no live SIP peer or
    input-automation tooling was available in this session (a background
    coding session, not the live two-device workflow used for earlier
    crash-fix work in this repo's history).
25. **UNSUPPORTED**: none — everything NOT RUN is an environment
    constraint, not a missing/unsupported feature.
26. **Regresii**: zero — 79/79 CTest passed after every commit in this
    task, Debug and Release.
27. **Limitări**: (a) single active call only, app-wide (pre-existing,
    confirmed via `SipManager::m_activeCall`, unrelated to this task) — "aceeași
    contact, multiple dialogs" is therefore not applicable, same as W111's
    documented limitation; (b) unread-read cursor is session-only, not
    persisted (intentional — see `docs/conversation-model.md`); (c)
    `PresenceStore` keys by raw entity URI, not case-normalized — a
    case-differing presence subscription could theoretically miss a match
    against a normalized conversation peer key (pre-existing, not
    introduced or fixed by this task); (d) no dedicated file-transfer-state
    column in `ConversationRow` yet (reuses existing fields).
28. **Probleme client**: none found beyond items 16b (recursion risk,
    fixed before it could manifest) and the pre-existing presence-key-casing
    note in item 27c.
29. **Probleme server**: none — no `SipManager`/`SipCall`/PJSIP changes in
    this task.
30. **Probleme de mediu**: no input-automation/second-peer tooling
    available for the interactive manual pass (item 24); not a code defect.
31. **Documentație**: `docs/conversation-workspace.md`,
    `docs/conversation-model.md`,
    `docs/conversation-state-and-correlation.md` (all new);
    `docs/release-notes.md` (new `v1.5.0` entry),
    `docs/versioning-and-rollout.md` (branch-model staleness + version-drift
    note), `docs/project-status.md` (W112 row + version line) — all updated.
32. **Commituri**: `f901106` (version bump, separate commit per the
    roadmap's rule), `4ee9256` (Conversation Workspace feature) — both on
    `feature/w112-conversation-workspace`, off
    `feature/w111-client-messaging-and-tools-navigation`.
33. **Git status**: clean at time of writing (only the two untracked
    diagnostics zip files pre-existing in the repo root from earlier live
    testing, untouched, not part of this task).
34. **Push status**: `git push -u origin feature/w112-conversation-workspace`
    executed after this report was written.
35. **Confirmare fără merge**: confirmed — no merge into `main`/`release`
    was performed.
36. **Confirmare privind pjproject**: confirmed — no file under any
    vendored PJSIP/pjproject directory was touched.
37. **Confirmare versiuni sincronizate**: Application/Backend/Frontend-UI
    all report `1.5.0` from the single `CMakeLists.txt`
    `project(VERSION)` source (via generated `AppVersion.h`) — no
    divergence; the pre-existing `v1.4.1` release-notes-status/tag mismatch
    (item 6) is a distinct, prior issue, explicitly flagged rather than
    silently fixed as part of this unrelated task.
38. **Confirmare informații de versiune**: startup log
    (`Application starting v1.5.0 (commit <hash>, <BuildType>)`), About
    dialog (now shows Version/Commit/Build — previously showed no version
    at all, a gap found and fixed this task), Diagnostics Center panel
    (already wired to `AppVersion.h` since before this task), diagnostics
    JSON/bundle export (`appVersion` field, already wired). Not added to
    every page/the Client view itself — matches the roadmap's own "don't
    clutter the Client view with versions" guidance (that's W115's
    System-Info-placement job, not this task's).
39. **Ce rămâne pentru W113**: the full Call Workspace overhaul (call
    header, media state consolidation, hold/resume/mute/camera controls,
    stats, single source of truth for call state) — this task deliberately
    left the existing call-control column untouched beyond wiring one
    "start a call from this conversation" action into it. Also carries
    forward from this task: a live two-device interactive manual pass of
    the Conversation Workspace; deciding whether to add a dedicated
    file-transfer-state `ConversationRow` field; whether `PresenceStore`
    should key on normalized URIs.
