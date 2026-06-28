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
| `feature/web-ui-tabs-sip-ladder-details` | Current active feature branch |
| `feature/*` | One branch per feature group; merged via PR |

---

## Future MAJOR releases (roadmap items)

These items are explicitly deferred to a future MAJOR version because they change the SIP wire protocol or introduce a new protocol stack:

| Item | Reason for MAJOR |
|------|-----------------|
| **LMPE real — ETSI TS 103 698** | Introduces new SIP body types, new SDP `m=application` negotiation, and ETSI-specific headers not present in current sessions. Changes the SDP contract with Zoiper/Linphone. |
| **Zoiper / Linphone compatibility matrix for messaging protocols** | Requires interop testing matrix across RTT (RFC 4103), SIP MESSAGE (RFC 3428), MSRP (RFC 4975), and LMPE. Each protocol path changes call setup and media answer logic. |
| **Full protocol negotiation — RTT / LMPE / SIP MESSAGE / MSRP** | Complete multi-protocol offer/answer engine. Incompatible with the current single-protocol re-INVITE consent flow. Architectural rewrite of `onCallRxReinvite` and `requestCallRtt`. |

These items must not be partially implemented on MINOR branches — any stub or infrastructure work must remain non-functional (UI label only, no SDP emission).
