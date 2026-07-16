# Agent Prompt — Task W111: Client Workspace Messaging Integration and Tools Navigation Consolidation

Condensed from the full task instructions (22 phases, permanent rules list,
48-item final report checklist — provided directly by the project owner in
chat; this file summarizes it for the repo's agent-prompt/agent-result
convention rather than duplicating every line).

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `feature/w111-client-messaging-and-tools-navigation`.
- Starting branch as literally specified: `fix/w109a-rtt-renegotiation-and-rtp-port-collision`.
  **Substituted** per the task's own "if a newer, complete, stable branch
  exists, start from that instead" clause: `audit/w110-full-project-code-review`
  (current HEAD at task start) is 10 commits ahead of, and a descendant of,
  the literally-specified branch, and contains a full session of live
  two-device crash/RTT/video fixes (all 77/77 CTest, Debug+Release). Starting
  from the older branch would have meant redoing or losing that work.
- No merge into `main`/`release`.

## Objective

1. Move every testing/diagnostic/developer-facing page out of the primary
   left nav into a single `Tools` section.
2. Integrate all existing messaging functions (SIP MESSAGE, MSRP, automatic
   transport, CPIM, IMDN, is-composing, Presence, file transfer, history,
   delivery status, transport selection, relevant fallback diagnostics)
   directly into the main Client view.
3. Clearly show the user/contact's real state and the real messaging state.

Explicit rules: no hardcoded IPs/domains/ports/accounts/URIs/paths; no
duplicated parsers/stores/controllers/transport logic; no separate
messaging implementation for the Client view — reuse
`MessagingEventStore`/`MessageHistoryStore`/`MessagingTransportPolicy`/
`PresenceStore`/IMDN correlation/is-composing controller/MSRP session-
transaction stores/file-transfer model/`SipManager`/`SipCall`/diagnostics
stores exactly as they exist; no UI updates directly from PJSIP/socket/
worker-thread callbacks (signals/slots, queued connections only); no UI
thread blocking; keep the app buildable after each major phase; run all
existing tests and add regression tests; never mark MSRP/relay/LMPE/file
transfer as production-ready; LMPE stays disabled by default.

## Phases (as given)

22 phases: baseline + inventory of the current left-nav pages, classify
each as user-facing vs. technical/diagnostic; add the `Tools` nav section
(sub-tabs for Diagnostics/Logs/SIP Ladder/Packet Capture/MSRP/Presence/
XCAP/Exports/Interop/Test-Developer, adapted to what actually exists — no
invented empty tabs); remove the moved items from the primary nav, update
routing/deep-links; Client-view messaging-mode selector (exclusive, backed
by the real `MessagingTransportPolicy` enum); content-type selector (Plain/
HTML/CPIM) + IMDN request controls; composer (send, content type, IMDN,
is-composing, file transfer, transport display, error display, retry);
full conversation history with per-transport status differentiation
(raw SIP/MSRP stays in Tools, not Client view); Presence display bound to
`PresenceStore`; is-composing indicator; file transfer action; compact
selected/actual/fallback transport + session status display; capabilities
model per contact/conversation; separating messaging availability from
call state; multiple-contact/multiple-call isolation; strict layering
(widgets present/emit intent/observe models — no protocol logic in
widgets); Tools routing/deep-links; UI persistence (with an explicit
do-not-persist list: drafts, credentials, raw payloads, relay paths,
nonces); stable automation IDs; automated tests per area; manual GUI test
pass reported PASS/FAIL/BLOCKED/NOT RUN/UNSUPPORTED; documentation; a
48-item final report; push (no merge).

## Deviations from the literal spec, and why

- **No Python test suite exists in this repo** (confirmed by Task W110's
  own audit, `docs/reviews/W110-full-project-code-review.md`) — the
  "run all Python tests" steps are reported N/A rather than fabricated.
- **Packet Capture / a generic Exports-Interop-Test-Developer tab** don't
  exist as distinct pages in this codebase — not invented as empty tabs;
  the closest existing equivalents (SIP Ladder, Diagnostics Center) are
  used instead.
- **Automation IDs on the wrapped Tools sub-tab widgets** (e.g.
  `toolsSipLadder`, `toolsLogs`) were **not** applied as literally listed:
  those panel classes already `setObjectName()` themselves for existing
  QSS styling (e.g. `DiagnosticsPanel` → `#DiagnosticsPanel` in
  `ThemeManager.cpp`), and overwriting that would have silently broken
  their theming. `NavRail` buttons keep objectName `"NavButton"` for the
  same reason (`#NavButton` checked/hover QSS); stable per-button ids are
  exposed via `accessibleName` instead. All Client Messaging View ids from
  the spec (`messagingContactSelector`, `messagingPresenceIndicator`, etc.)
  were applied as literally listed, since no pre-existing styling
  conflicted there.
- **Transport-mode persistence is global, not per-conversation** — see
  [../client-messaging-transport-selection.md](../client-messaging-transport-selection.md)
  for the reasoning (no per-peer settings store exists anywhere in this
  codebase; adding one solely for this would duplicate persistence
  machinery the task itself warns against).

## Final report / git requirements

Full 48-point final report (branch, versions, schemas, baseline, inventory,
Tools structure/routing, Client view architecture, transport modes,
content types, CPIM/IMDN/is-composing/Presence/history/file-transfer,
capabilities, multi-contact/call isolation, threading, accessibility IDs,
persistence, tests, manual pass results, regressions, limitations, next
steps, commits, `git status`, push status, no-merge confirmation, pjproject
untouched confirmation) — see
[../agent-results/W111-client-messaging-and-tools-navigation-result.md](../agent-results/W111-client-messaging-and-tools-navigation-result.md).
`git push -u origin feature/w111-client-messaging-and-tools-navigation`,
no merge to `main`/`release`.
