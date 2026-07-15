# Agent Prompt — Task W110: Full Project Code Review, Architecture Audit and Bug Discovery

Condensed from the full task instructions (28 phases, full permanent-rules
list, mandatory Romanian confirmation block, 57-item final report
checklist — provided directly by the project owner in chat; this file
summarizes it for the repo's agent-prompt/agent-result convention rather
than duplicating every line).

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `audit/w110-full-project-code-review`.
- Starting branch: the most recent stable branch in the chain,
  `fix/w109a-rtt-renegotiation-and-rtp-port-collision` (confirmed via
  `git branch -a`/`git log --oneline --decorate -30` — no separate `W109`
  branch exists).
- No merge into `main`/`release` (neither exists in this repo).

## Objective

Full-project review — not just the latest changes — across source,
config, build system, tests, docs, and cross-module integration. Find real
bugs (lifecycle, race conditions, memory issues, threading, SIP/SDP/media
negotiation errors, impossible states, unapplied config, hardcoded values,
duplicated code/parsers, UI/backend divergence, security gaps,
interoperability issues, tests that validate incorrectly, dead code, and
docs-vs-implementation drift). Every finding must be evidence-based (code,
tests, logs, reproducible behavior) — no assumed-working features.

Permanent rules: no hardcoded IPs/domains/ports/accounts/credentials/URIs/
local paths; do not modify pjproject unless unavoidable and separately
justified; no large changes before the audit report is finished; no
workarounds hiding problems; no stylistic-only architecture changes; do not
block the UI thread; PJSIP/socket/worker callbacks must not touch UI
directly; protocol data processed binary-safe with `QByteArray`/byte
sizes; never log credentials/Authorization/digests/nonces/tokens/sensitive
paths/private keys.

## Phases (as given)

28 phases: baseline (build Debug/Release/`ENABLE_PJSIP=ON`, all
tests/harnesses), project inventory (modules/flows/ownership/threads),
build-system review, lifecycle/ownership review, threading/UI-
responsiveness review, SIP/dialog-state review, SDP/media-negotiation
review, audio review, video review, RTT review, SIP MESSAGE/messaging
review, MSRP-direct review, MSRP-relay review, file-transfer review,
Presence/XCAP review, parser/binary-safety review, config/profile review,
full UI review, logging/diagnostics/export review, test-suite audit,
static analysis, security audit, docs-vs-code comparison, finding
classification (unique `W110-F0xx` IDs with severity/evidence/repro/fix),
permitted-fixes phase (Critical/High + small isolated fixes only, each with
its own regression test and before/after evidence), post-fix retest,
documentation, and a remediation roadmap for everything not fixed in this
task.

## Final report / git requirements

57-item structured report (branch, root causes, architecture, per-subsystem
coverage, findings counts by severity, fixes applied, regressions, roadmap,
git status). Push `audit/w110-full-project-code-review` to origin; do not
merge; explicitly confirm the review covered the whole project (not just
recent changes), every finding is evidence-based, no interoperability was
declared without a real test, large unfixed issues were separated into
dedicated follow-up tasks, pjproject sources were not modified (or any
change is justified), and no sensitive information was introduced.

See [W110-full-project-code-review-result.md](../agent-results/W110-full-project-code-review-result.md)
for the actual outcome, and [docs/reviews/](../reviews/) for the full
findings, architecture map, test-coverage-gaps, security review, and
remediation roadmap produced by this task.
