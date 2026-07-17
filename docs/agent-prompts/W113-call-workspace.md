# Agent Prompt — Task W113: Call Workspace

Condensed from the full W112–W117 roadmap ("ROADMAP W112–W117 — Product
Stabilization, Versioning and Release Readiness", provided by the project
owner in chat) — this file summarizes the W113-specific section plus the
roadmap's global rules, rather than duplicating every line.

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `feature/w113-call-workspace`.
- Starting branch: `feature/w112-conversation-workspace` (W112, complete,
  79/79 CTest, pushed without merge).
- No merge into `main`/`release`.

## Objective

Consolidate every call control into a coherent workspace integrated with
the conversation. Required: call header, remote identity, presence, call
duration, call state, audio/video/RTT media state, hold/resume, mute,
camera on/off, request/remove video, request/accept/reject RTT, media
device status, jitter/loss/RTT stats, selected media, negotiated media,
actual media, session diagnostics summary, access to SIP Ladder via Tools,
multiple-call isolation, graceful degradation.

Constraints: one single source of truth for call state; UI must not infer
active media from a button's checked-state; hold/resume must reflect SDP
and real media; request video/RTT must use a correct state machine; stats
update only while the stream exists; no `getInfo()` call after session
termination without a guard; no duplicate conference-port connections;
audio/video/RTT isolated per call; clear camera-preview/call-capture
ownership; multiple calls selectable without state mixing.

## Global rules (apply to every task in the roadmap, W112–W117)

Mandatory version bump every task (no two consecutive tasks may keep the
same version); SemVer MAJOR.MINOR.PATCH; a separate commit for the version
bump; Debug + Release + `ENABLE_PJSIP=ON` builds; all existing tests + new
tests; docs; `docs/project-status.md` update; agent-prompt/agent-result
pair; push (no merge); no pjproject changes without explicit justification;
no hardcoded environment-dependent values; no interoperability claims
without a real test; never report PASS for a NOT RUN/BLOCKED test;
experimental features stay disabled by default.

## Version

Current baseline (post-W112) is `1.5.0` — this task bumps MINOR to
**1.6.0**, matching the schedule W112 itself projected for W113.

## Real gap found during research (folded into this task per project-owner decision)

`src/gui/panels/CallPanel.{h,cpp}` was a fully-built call-control widget —
compiled (in `CMakeLists.txt`) but never instantiated anywhere in the app —
and the only place emergency-call UI existed. This meant emergency calling
was unreachable in the shipped app even with
`AppSettings::emergencyTestModeEnabled()` on. Per the project owner's
explicit choice (asked via a scoped decision, not assumed), this task
folds `CallPanel`'s emergency-call section (still gated behind the same
default-off setting), audio-codec card, and selected-media card into the
new `CallWorkspacePanel`, then deletes the now-fully-superseded
`CallPanel.{h,cpp}`.

## Scope boundary

This task does not add real concurrent multi-call support — `SipManager`
is single-active-call by architecture (confirmed, no dead scaffolding for
multiple calls exists). "Multiple call isolation" is delivered as rigorous
state reset between sequential calls via `CallInfoModel::reset()`, matching
the same documented limitation already used for this in W111/W112.

## Final report / git requirements

Full report per the roadmap's 39-item template — see
`docs/agent-results/W113-call-workspace-result.md`.
`git push -u origin feature/w113-call-workspace`, no merge.
