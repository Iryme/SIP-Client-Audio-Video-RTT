# Versioning and Rollout Policy

## Semantic Versioning

This project follows [Semantic Versioning 2.0.0](https://semver.org/) — `MAJOR.MINOR.PATCH`.

| Type | When to use | Examples |
|------|-------------|---------|
| **PATCH** | Bugfix with no behavioral change visible to the user or the SIP peer. Regression fixes, assert/crash fixes, UI cosmetic corrections. | Fix camera freeze on call end, fix Qt::UniqueConnection assert, fix hold button state |
| **MINOR** | New functionality added in a backward-compatible way. New UI panels, new signal flows, new media controls, new SIP features that do not break existing call flows. | Dashboard Quick SIP Actions, MediaRequestDialog popups, RTT consent flow, RTP stats in status bar |
| **MAJOR** | Incompatible change, architectural rewrite, or addition of a new major protocol that alters the SIP signaling contract with remote peers. | LMPE real (ETSI TS 103 698), full RTT/LMPE/SIP MESSAGE/MSRP protocol negotiation matrix, wire-level SDP changes incompatible with current Zoiper/Linphone sessions |

### Rules

- Every task proposal must declare its version bump: PATCH / MINOR / MAJOR.
- A MINOR bump accumulates all PATCH fixes on the same branch before tagging.
- A MAJOR bump requires a design review commit (`docs/architecture-decisions.md`) before implementation begins.

---

## Rollout Gate

A version may be tagged and released **only after** all three gates pass:

1. **Full build** — `codex_build_vsdev` exits 0 on a clean checkout.
2. **Automated tests** — all ctest tests pass (currently 12/12: `test_sip_manager`, `test_registration_retry`, `test_registration_expiry`, `test_profile_switch`, and others).
3. **Manual validation** — checklist in the corresponding release notes entry is signed off.

### Rollout steps

```
1. Ensure working tree is clean (git status)
2. Run full build + ctest
3. Manual validation against the checklist
4. git tag -a vMAJOR.MINOR.PATCH -m "Release vMAJOR.MINOR.PATCH"
5. Update docs/release-notes.md (status → released, add tag)
6. Commit release notes update: "Release vMAJOR.MINOR.PATCH"
7. Merge feature branch → main/release branch (PR, no force-push)
```

**Nu face push** until all gates pass and the maintainer approves.

---

## Branch model

| Branch | Purpose |
|--------|---------|
| `feature/project-skeleton` | Integration branch (treated as `main` for this project phase) |
| `feature/*` | One branch per feature group; merged via PR |

> **Task W112 update**: the table above had gone stale — it still named
> `feature/web-ui-tabs-sip-ladder-details` as "current active feature
> branch," a branch long superseded by the actual chain of work
> (`feature/w1xx-*` task branches, each starting from the previous one,
> current tip `feature/w112-conversation-workspace`). This policy doc
> tracks the general *model* (one branch per feature group), not a
> snapshot of whichever branch happens to be active — no attempt is made
> here to keep a literal "current branch" cell up to date going forward;
> `docs/project-status.md`'s "Active branch" line is the authoritative
> place for that.
>
> Also reconciled this task: `CMakeLists.txt`'s `project(VERSION)` and
> `Application.cpp`'s `setApplicationVersion()` had drifted to `0.1.0`,
> disconnected from the real tagged release history this file's SemVer
> policy governs (`v1.2.0`…`v1.4.1`). Fixed as of `v1.5.0` — see
> [release-notes.md](release-notes.md)'s `v1.5.0` entry. Separately, the
> `v1.4.1` entry in release-notes.md still says "Status: in development"
> even though a `v1.4.1` git tag already exists — flagged, not silently
> rewritten, since this task did not re-run that release's own rollout gate.

---

## Portable distribution artifacts

Starting with **v1.6.4** (Task W113D), a portable Windows x64 Release bundle
is produced alongside each release via
[scripts/package-windows.ps1](../scripts/package-windows.ps1) — see
[windows-portable-bundle.md](windows-portable-bundle.md). The bundle's
`applicationVersion`/`backendVersion`/`frontendUiVersion` in its
`version-info.json` are always identical, since SIPClient is a single binary
with a single `PROJECT_VERSION` (no separate backend/frontend build). The
Windows binary's own `FILEVERSION`/`PRODUCTVERSION` resource
(`cmake/AppVersion.rc.in`, added in W113D) is generated from the same
`PROJECT_VERSION` as the in-app About dialog and diagnostics export, so
`git tag vMAJOR.MINOR.PATCH` for a release should never drift from what
Explorer's Properties > Details tab or the packaged bundle reports.

Starting with W113I, platform releases are numbered independently. The CMake
`PROJECT_VERSION` remains the Windows version and feeds the Windows resource;
`SIPCLIENT_APP_VERSION` overrides application/runtime/bundle metadata only on
Apple. Thus this branch reports macOS 1.6.9/build 169 while Windows remains
1.6.8. A common-code fix does not imply that Windows has shipped it: Windows
version metadata changes only in its separate build and regression task.

Starting with **v1.6.8** (Task W113G), the corresponding Apple Silicon
artifact is produced by `scripts/package-macos.sh`. It uses the same single
platform application version, records the native architecture, deployment target, Qt and
PJSIP versions, commit, signing mode, and notarization status in both the app
and distribution manifest, and publishes a SHA-256 sidecar. A Developer-ID
signature, successful notarization, and clean-machine result remain release
gates; an ad-hoc signed engineering artifact does not satisfy those gates.

## Future MAJOR releases (roadmap items)

These items are explicitly deferred to a future MAJOR version because they change the SIP wire protocol or introduce a new protocol stack:

| Item | Reason for MAJOR |
|------|-----------------|
| **LMPE real — ETSI TS 103 698** | Introduces new SIP body types, new SDP `m=application` negotiation, and ETSI-specific headers not present in current sessions. Changes the SDP contract with Zoiper/Linphone. |
| **Zoiper / Linphone compatibility matrix for messaging protocols** | Requires interop testing matrix across RTT (RFC 4103), SIP MESSAGE (RFC 3428), MSRP (RFC 4975), and LMPE. Each protocol path changes call setup and media answer logic. |
| **Full protocol negotiation — RTT / LMPE / SIP MESSAGE / MSRP** | Complete multi-protocol offer/answer engine. Incompatible with the current single-protocol re-INVITE consent flow. Architectural rewrite of `onCallRxReinvite` and `requestCallRtt`. |

These items must not be partially implemented on MINOR branches — any stub or infrastructure work must remain non-functional (UI label only, no SDP emission).
