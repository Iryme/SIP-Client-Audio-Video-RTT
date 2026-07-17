# Agent Prompt — Task W112: Conversation Workspace

Condensed from the full W112–W117 roadmap (provided directly by the
project owner in chat, "ROADMAP W112–W117 — Product Stabilization,
Versioning and Release Readiness") — this file summarizes the W112-specific
section plus the roadmap's global rules for the repo's agent-prompt/
agent-result convention, rather than duplicating every line.

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `feature/w112-conversation-workspace`.
- Starting branch: `feature/w111-client-messaging-and-tools-navigation`
  (W111, complete, 78/78 CTest, pushed without merge).
- No merge into `main`/`release`.

## Objective

Transform the Clients page into a contact/conversation-centric workspace:
model Contact → Conversation → Messaging → Call, explicitly not
Call → Conversation. Required: conversation list, selected contact, last
message, timestamp, unread count, presence, typing indicator, actual
transport, call state, file transfer state, search, filters, favorite/
pinned (if the existing model allows), full isolation between
conversations, persistent history per the existing architecture, deep
links to Tools, a call action started from the conversation.

Constraints: one central ConversationModel/ViewModel; no protocol logic in
widgets; no duplicating `MessageHistoryStore`; no shared is-composing
controller between conversations; multiple conversations with the same
peer must correlate correctly; unread count must update deterministically;
switching conversations must never mix typing/history/file-transfer/
presence; conversations without a call must still allow SIP MESSAGE; MSRP
only when a session is actually negotiated; Unknown and Offline states must
never be confused.

## Global rules (apply to every task in the roadmap, W112–W117)

Mandatory version bump every task (no two consecutive tasks may keep the
same version); SemVer MAJOR.MINOR.PATCH; a separate commit for the version
bump; Debug + Release + `ENABLE_PJSIP=ON` builds; all existing tests + new
tests; docs; `docs/project-status.md` update; agent-prompt/agent-result
pair; push (no merge); no pjproject changes without explicit justification;
no hardcoded environment-dependent values; no interoperability claims
without a real test; never report PASS for a NOT RUN/BLOCKED test;
experimental features stay disabled by default.

## Version bump — audited and reconciled, not applied blindly

The roadmap's own suggested schedule assumes the current version is
`0.1.0` (W112 → 0.2.0, …, W117 → 1.0.0-rc.1). An audit of this repo (this
task's first step) found that assumption **false**: `CMakeLists.txt`'s
`project(VERSION)` was indeed still `0.1.0`, but it had simply never been
updated — `docs/release-notes.md` and existing git tags (`v1.2.0` through
`v1.4.1`) show a real, already-shipped version history far past `0.1.0`.
Per the roadmap's own override clause ("if the audit shows a different
existing convention, respect it — don't invent a parallel scheme"), this
task bumps from the real baseline (`1.4.1`) instead: **1.4.1 → 1.5.0**
(MINOR — new compatible feature). See
`docs/agent-results/W112-conversation-workspace-result.md` for the full
audit and the projected W113–W117 schedule under this corrected baseline.

## Scope boundary

W113 ("Call Workspace") owns the full call-control UI overhaul. This task
only inverts the Clients page's *navigation* model (conversation-first)
and adds a "start a call from this conversation" action — it does not
redesign the status-card grid, video, or RTT columns.

## Final report / git requirements

Full report per the roadmap's 39-item template (task, branches, commits,
versions across Application/Backend/Frontend-UI/Schema/Build/Commit,
architecture, features, bugs found/fixed, tests, manual/E2E results with
explicit PASS/FAIL/BLOCKED/NOT RUN/UNSUPPORTED, regressions, limitations,
docs, git status/push/no-merge/pjproject confirmations, what's left for
W113) — see
`docs/agent-results/W112-conversation-workspace-result.md`.
`git push -u origin feature/w112-conversation-workspace`, no merge.
