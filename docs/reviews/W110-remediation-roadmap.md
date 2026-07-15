# W110 Remediation Roadmap

Findings not fixed in W110 (per its own rule: only Critical/High + small,
clearly isolated fixes are permitted; architectural changes are deferred to
dedicated tasks). Grouped by theme, ordered by suggested execution order.
See [W110-findings.md](W110-findings.md) for full evidence on every
referenced finding ID.

## W111 — Call State Machine and RTT Consent Hardening
**Scope:** W110-F006.
**Branch:** `fix/w111-call-answer-rtt-consent`
**Risk:** Low-medium — touches the initial-INVITE answer path, which is
exercised by every inbound call.
**Order:** Can run independently of W112–W115.
**Dependencies:** None.
**Acceptance criteria:**
- `SipCall::answer()` gates `prm.opt.textCount` on the profile's RTT
  setting (or routes the initial-offer RTT request through the same
  consent UI as re-INVITE-time offers — pick one, document the choice).
- Regression test covering both "RTT disabled in profile, remote offers
  m=text in initial INVITE" and "RTT enabled" cases.
- Full existing CTest suite still green; no regression in audio/video/MSRP
  initial-offer-at-call-creation behavior (the behavior Task W109A
  explicitly protected).

## W112 — MSRP Relay Allocation Lifecycle Fix
**Scope:** W110-F001, W110-F017 (same class/file, bundle for efficiency).
**Branch:** `fix/w112-msrp-relay-allocation-lifecycle`
**Risk:** Medium — touches the relay refresh/re-adoption path, which only
manifests on long-duration relay-mode calls; regression risk is contained
to MSRP relay mode (direct MSRP and non-relay calls unaffected).
**Order:** After W111.
**Dependencies:** None.
**Acceptance criteria:**
- `MsrpRelayClient`'s refresh either refreshes the existing connection in
  place, or `MsrpSession` re-adopts a newly refreshed transport and
  triggers SDP re-negotiation (`a=path` update) — with the chosen design
  documented in `docs/msrp-relay-allocation.md`.
- `MsrpCallPreparationController::start()` disconnects its previous
  `m_timeoutTimer` connection before reconnecting (W110-F017).
- A test simulating allocation-TTL-based refresh confirms message delivery
  continues to work after a refresh cycle.
- Manual same-host or live-relay retest of a long-duration MSRP relay call,
  or explicit NOT RUN/BLOCKED with reason if no long-lived test relay is
  available.

## W113 — File Transfer Integrity and MSRP Session Cleanup
**Scope:** W110-F003, W110-F007, W110-F008, W110-F016.
**Branch:** `fix/w113-file-transfer-integrity-and-msrp-cleanup`
**Risk:** Medium — touches the file-receive completion path and adds new
timers to `MsrpSession`; needs care not to regress existing file-transfer
and messaging tests.
**Order:** After W112 (shares `MsrpSession`/`MsrpChunkAssembler` context).
**Dependencies:** None strictly, but easier once W112 lands.
**Acceptance criteria:**
- Negotiated file-selector hash is threaded through to reassembly
  completion and actually checked via `MsrpFileReceiver::verifyHash()`,
  with a visible warning/rejection on mismatch (W110-F003).
- `MsrpChunkAssembler::purgeStale()` is invoked periodically; abandoned
  chunked transfers are evicted and reported as failed (W110-F007).
- Outbound MSRP SEND transactions time out and reach `TimedOut` with a
  user-visible failure instead of pending forever (W110-F008).
- Suggested filenames screen for Windows reserved device names
  (W110-F016).
- New regression tests for hash-mismatch rejection, stale-buffer eviction,
  and transaction timeout; full CTest suite still green.

## W114 — Messaging Consistency and Documentation Follow-ups
**Scope:** W110-F009, W110-F010, W110-F015.
**Branch:** `fix/w114-messaging-consistency`
**Risk:** Low — isolated changes to `MessageHistoryStore::correlateDelivery`,
`CpimBuilder`, and two XML parsers' size guard.
**Order:** Independent; can run any time after W110.
**Dependencies:** None.
**Acceptance criteria:**
- `correlateDelivery` never downgrades `Displayed` to `Delivered`
  (W110-F009), with a regression test for out-of-order IMDN delivery.
- `CpimBuilder` emits RFC 3862 CRLF line endings (W110-F010); retested
  against a live interop peer per this project's existing interop-testing
  convention (not just unit tests), since this is a wire-format change.
- `ImdnParser`/`IsComposingParser` gain the same input-size cap
  `PidfParser` already has (W110-F015).

## W115 — PJSIP Real-Backend Test Coverage and Dead-UI Cleanup
**Scope:** W110-F005, W110-F011, and the deferred deeper GUI panel-by-panel
audit noted in W110-findings.md's coverage notes.
**Branch:** `chore/w115-pjsip-test-coverage-and-dead-ui`
**Risk:** Low functional risk (mostly test infrastructure + removing dead
UI fields), but nontrivial effort to design a mockable pjsua2 seam or a
loopback-UAS test harness.
**Order:** Can run in parallel with W111–W114; independent scope.
**Dependencies:** None.
**Acceptance criteria:**
- At least a curated subset of `HAVE_PJSIP`-gated assertions (currently
  `QSKIP`'d) run under CTest against either a mock pjsua2 seam or a local
  loopback SIP UAS, covering registration success/failure and basic
  call setup/teardown at minimum.
- `live_*_probe` executables are either registered as opt-in CTest targets
  (clearly labeled as requiring network/live infrastructure) or their
  coverage is explicitly superseded by the new tests above.
- Dead "Connection" fields in `SettingsPanel` (W110-F011) are removed or
  wired to something real, with a decision recorded in
  `docs/sip-profiles.md` or `docs/gui-layout.md`.

## Not scheduled as a task (repo-hygiene decision, needs owner confirmation)

- **W110-F013** — `CMakeUserPresets.json` tracked in git with a
  machine-specific path. Untracking it (`git rm --cached` +
  `.gitignore`) is a one-line change but affects how every contributor
  configures their local build; left for the project owner to decide
  rather than bundled into any of the above.
- **W110-F018** — self-documented test-order dependency in
  `test_diagnostics_bundle.cpp`. Low risk, single binary; worth a
  five-minute fix (e.g. reset the singleton in `init()`) whenever that
  file is next touched, not worth its own task.
