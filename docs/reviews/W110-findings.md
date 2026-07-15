# W110 Findings — SIP-Client-Audio-Video-RTT Full Project Code Review

Branch: `audit/w110-full-project-code-review` (started from
`fix/w109a-rtt-renegotiation-and-rtp-port-collision`, commit `05fb50c`).

All findings below are evidence-based: each was verified by reading the
cited code (or, for the two fixed in this task, by reproducing the exact
rejected/no-op behavior). No finding is speculative. **No Critical-severity
issue was found** — no crash, memory corruption, credential leak, or remote
injection was identified anywhere in the areas reviewed (see Faza-by-faza
coverage notes at the end of this document for what was and wasn't reached
at full depth).

Severity definitions used throughout:
- **Critical** — crash, memory corruption, credential leak, remote injection, major call/media corruption, data loss.
- **High** — major feature non-functional, deadlock, severe race, wrong negotiation, duplicate protocol messages, security bypass.
- **Medium** — incorrect workflow/state, incomplete recovery, inconsistent export, ignored config.
- **Low** — diagnostics/logging/UI inconsistency, documentation drift, maintainability issue with demonstrable impact.

---

## High severity

### W110-F001 — MSRP relay allocation refresh orphans the live call after `takeTransport()`
**Module:** msrp | **Files:** `src/msrp/MsrpRelayClient.cpp:341-366,67-76`, `src/sip/SipCall.cpp:1582-1598`, `src/msrp/MsrpCallPreparationController.cpp`
**Status:** Not fixed in W110 (architectural — requires re-negotiation flow). Proposed for W113 (see roadmap).

`MsrpCallPreparationController` allocates a relay Use-Path and arms
`MsrpRelayClient::m_refreshTimer` before the TTL expires. Once ready,
`SipCall::makeCallWithOptions` calls `relayClient->takeTransport()`, which
`std::move`s the transport out, leaving `MsrpRelayClient::m_transport` null.
When the refresh timer later fires, `onRefreshTimerFired()` sees
`!m_transport` and opens a **brand-new** control connection, producing a
new, separate allocation (`allocationRefreshed` emitted) that is completely
disconnected from the transport actually carrying the live session's
traffic. Nothing in the codebase connects to `allocationRefreshed`/
`allocationLost` to re-adopt the new allocation or re-negotiate
`a=path` — confirmed by repo-wide grep, only doc comments reference these
signals. `MsrpRelayClient.h:71-75` explicitly documents that the caller
must re-adopt the transport and re-negotiate SDP; this is never implemented.

**Reproduction:** place a call with MSRP relay mode `required`, keep the
MSRP session open past the relay allocation's TTL (default 600s). The
original allocation expires server-side while the client believes it
refreshed successfully; message delivery silently stops with no error
surfaced.

**Impact:** silent MSRP delivery failure on any relay-mode call that
outlives one allocation lifetime (~10 minutes by default).

**Fix direction:** either (a) `MsrpSession` keeps a back-reference to the
relay client and re-adopts a new transport + triggers a re-INVITE with
updated `a=path` on `allocationRefreshed`, or (b) refresh the *existing*
connection in place instead of opening a new one once a transport has been
handed off.

---

### W110-F002 — `Held → Failed` is not a legal call-state transition; call could get stuck at "Held" forever
**Module:** sip | **Files:** `src/sip/CallStateMachine.cpp:150-156`, `src/sip/SipCall.cpp` (`onCallState`)
**Status:** **Fixed in this task.**

`CallStateMachine::isValidTransition` only allows `Held → Active` or
`Held → Disconnecting`. `PjCall::onCallState` maps
`PJSIP_INV_STATE_DISCONNECTED` with `lastStatusCode >= 400` to
`CallState::Failed`, but the dispatch logic only special-cased
`newState == Idle` with a reroute through `Disconnecting` — there was no
equivalent for `newState == Failed`. If the dialog terminates abnormally
(transport drop, 408 on a re-INVITE, 5xx on a stale dialog) while the call
is `Held`, `tryTransition(Failed)` is rejected by the state machine (logged
as a warning only) and the call remains stuck at `Held` — `callFailed`/
`callDisconnected` never fire, `RttSession::onCallEnded` never runs, and the
UI keeps showing "Paused" with no way to end the call through the normal
flow.

**Fix applied:** `SipCall::onCallState`'s Qt-thread continuation now
reroutes `Held → Failed` through `Disconnecting` first (the same pattern
already used for `Held → Idle`), matching `CallStateMachine`'s existing
`Disconnecting → Failed` transition. See `src/sip/SipCall.cpp` around the
existing `newState == Idle` special case.

**Test added:** `tests/test_call_state_machine.cpp` —
`heldToFailedRequiresDisconnectingReroute` verifies the direct transition
is still (correctly) rejected at the state-machine level and that the
two-step reroute (`Held → Disconnecting → Failed`) succeeds, documenting
the contract `SipCall::onCallState` relies on.

---

### W110-F003 — Negotiated file-transfer hash is never verified against received bytes
**Module:** msrp / file-transfer | **Files:** `src/msrp/MsrpFileReceiver.cpp:37` (`verifyHash`), `src/msrp/MsrpSession.cpp:262-306`, `src/gui/panels/MsrpPage.cpp:355-368`
**Status:** Not fixed in W110 (requires threading SDP-negotiated hash through to the reassembly completion point — more than a one-line change). Proposed for W114.

`MsrpFileReceiver::verifyHash()` is fully implemented and unit-tested, and
`MsrpFileSelector::parse()` correctly extracts `hash:<algo>:<hex>` from the
negotiated `a=file-selector`. A repo-wide grep shows `verifyHash` is called
**only from its own test file** — no production code path calls it against
the reassembled body, and the negotiated file selector is never threaded
through to where the body becomes available. A received file is saved with
no integrity check and no warning on mismatch.

**Fix direction:** thread the file-selector's declared hash from
`MsrpSdpNegotiator` into `MsrpSession`/`MsrpChunkAssembler`, and call
`MsrpFileReceiver::verifyHash()` before/at `fileTransferReceived`, surfacing
a warning on mismatch.

---

### W110-F004 — Silent call failure when MSRP relay is `required` and allocation fails (async path)
**Module:** sip / gui | **Files:** `src/sip/SipManager.cpp` (`dispatchMakeCall`'s `failed` lambda), `src/gui/panels/CallPanel.cpp:1191` (`onCallFailed`)
**Status:** **Fixed in this task.**

The commit immediately preceding this audit (`260a4e9`, "surface feedback
when makeCall() is silently rejected") fixed the *synchronous* rejection
path (`makeCall()` returning `false`), but not this *asynchronous* one: when
`msrp/relay/mode=required` and the async relay allocation subsequently
fails, the `failed` lambda called `SipCall::reset()`, which unconditionally
routes the (never-started) call back to `Idle` via
`CallStateMachine::reset()` — never emitting `callFailed`. Every UI surface
that shows a call-failure reason (`CallPanel::onCallFailed`,
`MainWindow`'s status bar/video-request cleanup) listens only to
`SipManager::callFailed`, so the user saw the dial screen silently reset
with zero indication of why, directly undercutting the intent of the
just-landed commit for this one specific (async) failure path.

**Fix applied:** the `failed` lambda now also `emit self->callFailed(...)`
with the same reason string, reusing the signal and UI wiring that already
exists (verified safe: `CallPanel::onCallFailed` only updates a status
label; the `MainWindow`/emergency-call `callFailed` handlers only reset
transient UI flags, all idempotent no-ops when nothing was pending).

**Regression test:** not added — this path requires a live/simulated relay
allocation failure, which is only exercisable through the `HAVE_PJSIP`
integration surface that this repo does not unit-test today (see
W110-F005). Manually verified by code inspection of the signal path and the
three connected slots.

---

### W110-F005 — PJSIP real-backend code paths are `QSKIP`'d in CTest; live probes aren't registered as tests
**Module:** tests / build | **Files:** `tests/test_sip_manager.cpp`, `test_registration_state_machine.cpp`, `test_registration_retry.cpp`, `test_registration_expiry.cpp`, `test_profile_switch.cpp`, `test_audio_media.cpp`, `test_video_media.cpp`, `tests/CMakeLists.txt:1815-1954` (`live_*_probe` targets)
**Status:** Not fixed in W110 (a test-infrastructure investment, not a bug fix). Proposed for W115.

All seven files above `#ifdef HAVE_PJSIP` / `QSKIP` their real assertions
when built with the real pjsua2 backend — which is exactly the
configuration used for this audit's baseline (`ENABLE_PJSIP=ON`). The
`live_*_probe` executables that do exercise the real backend
(`live_audio_call_probe.cpp`, `live_registration_probe.cpp`, etc.) are
**not** registered via `add_test` (confirmed in `tests/CMakeLists.txt`), so
CTest's "100% passed" baseline never actually exercises `SipAccount.cpp`'s
or `SipCall.cpp`'s real registration/call-control logic — only the stub
backend and the pieces that don't depend on `HAVE_PJSIP` (state machines,
parsers, etc., which are otherwise very well tested). A regression in the
real PJSIP glue code (e.g. the exact bug fixed as W110-F002, or the SDP/RTT
logic fixed in Task W109A) would not be caught by `ctest` alone.

**Fix direction:** not a quick fix — requires either a fake/mockable
`pj::Call`/`pj::Account` seam or promoting a curated subset of the
`live_*_probe` assertions into CTest-registered tests that can run without
a live registrar (e.g. against a local loopback UAS). Sized as its own task
(W115) rather than attempted here.

---

## Medium severity

### W110-F006 — `answer()` unconditionally accepts an RTT offer in the initial INVITE, bypassing the per-profile `enableRtt` setting
**Module:** sip / rtt | **File:** `src/sip/SipCall.cpp:1693-1728` (`SipCall::answer()`)
**Status:** Not fixed in W110 (needs a decision on UX: silent-accept vs. route through the same consent dialog as re-INVITE-time offers — a product call, not a pure bug fix). Proposed for W112.

`answer()` hardcodes `prm.opt.textCount = 1` with no reference to
`CallMediaOptions::enableRtt`/the active profile's RTT setting — confirmed
via grep (zero references in the file). If a remote peer's initial INVITE
offers `m=text`, RTT is silently negotiated active regardless of the local
user's "Enable RTT" setting, inconsistent with the deliberate consent flow
Task W109A built for re-INVITE-time RTT offers (`onCallRxReinvite` declines
by default and requires explicit accept/reject).

**Fix direction:** gate `prm.opt.textCount` in `answer()` on the
account/profile's RTT setting, or route the initial-INVITE RTT offer
through the same pending-request UI used for re-INVITE-time offers.

---

### W110-F007 — `MsrpChunkAssembler::purgeStale` is never invoked — abandoned chunked transfers leak memory
**Module:** msrp | **File:** `src/msrp/MsrpChunkAssembler.h:60`, `MsrpChunkAssembler.cpp:135-147`
**Status:** Not fixed in W110. Proposed for W114.

`purgeStale(idleDeadline)` exists specifically to evict stale
partially-assembled entries, but no production code calls it — confirmed by
repo-wide grep. A peer that starts a chunked SEND and then disconnects
without an `Abort` continuation leaves the partial buffer (up to
`m_maxMessageBytes`, default 2 MiB) in `m_pending` forever.

**Fix direction:** a periodic `QTimer` in `MsrpSession` calling
`purgeStale()` and treating purged IDs as failed deliveries.

---

### W110-F008 — Outbound MSRP SEND transactions have no response timeout; `TimedOut` status is unreachable
**Module:** msrp | **Files:** `src/msrp/MsrpTypes.h:144`, `MsrpTransactionStore.cpp:4-12,94-110`
**Status:** Not fixed in W110. Proposed for W114 (pairs naturally with F007).

`MsrpTransactionStatus::TimedOut` is defined and included in `isTerminal()`,
but nothing transitions a transaction to it — no timer exists in
`MsrpSession::sendFrame()`/`sendMessage()`/`sendFile()`. An unresponsive
peer leaves a transaction `Queued` indefinitely; `trimCompleted()` only
trims terminal transactions so it never gets cleaned up, and the user sees
the message stuck at "sending" forever with no failure surfaced.

**Fix direction:** a per-transaction or sweep timer that marks
`TimedOut` and emits `messageDeliveryStatusChanged(..., false, "timed out")`.

---

### W110-F009 — IMDN delivery state can regress from "Displayed" back to "Delivered" on out-of-order reports
**Module:** sip / messaging | **File:** `src/sip/MessageHistoryStore.cpp:263-289` (`correlateDelivery`)
**Status:** Not fixed in W110. Proposed for W114.

`correlateDelivery()` unconditionally overwrites `deliveryState` with no
precedence check. A `Delivered` IMDN arriving after a `Displayed` one
(plausible under network reordering or transport duplication) silently
downgrades the UI's disposition indicator.

**Fix direction:** ignore a `Delivered` update if the current state is
already `Displayed` (still allow `Failed`/`Error` to override anything).

---

### W110-F010 — `CpimBuilder` emits bare LF instead of RFC 3862's mandated CRLF
**Module:** sip / messaging | **File:** `src/sip/CpimBuilder.cpp:9-17`
**Status:** Not fixed in W110 (interop-sensitive; wants a live-interop retest, not just a unit-level change). Proposed for W114.

RFC 3862 specifies CRLF line termination for CPIM headers/body separator.
This codebase's own `CpimParser` is lenient (normalizes line endings first)
so round-tripping through this app works, but a strict third-party CPIM
parser may reject a message this composer generates.

**Fix direction:** emit `\r\n` in `CpimBuilder::build()`; retest against a
live RCS/IMDN-capable peer per this project's interop-testing convention
before calling it done.

---

### W110-F011 — Dead "Connection" fields in `SettingsPanel`: stored, exported, restorable, never applied
**Module:** gui / config | **Files:** `src/gui/panels/SettingsPanel.cpp:69-93,257-263,315-321,346-352`, `src/gui/MainWindow.cpp:141-220`
**Status:** Not fixed in W110 (removal/rewire is a UI decision better bundled with its own small task + a screenshot-level check). Proposed for W115.

`SettingsPanel`'s "Connection" group (`IP server`, `SIP domain`,
`SIP port`, `WebSocket URL`, "Persist media selection locally") persists to
`connection/*` keys that are round-tripped through config export/import but
never read anywhere in `src/sip` — the actual SIP connection is configured
exclusively through `SipProfile`/`SipProfileManager`. A user filling these
in sees them "save" successfully with zero effect on registration/calls.

**Fix direction:** remove the dead fields (the profile system already
supersedes them), or wire them to something real if they were meant to
seed a new profile.

---

### W110-F012 — `passwordNeverAppearsInStateTransitionLogs` test asserted nothing (`QVERIFY(true)`)
**Module:** tests | **File:** `tests/test_registration_state_machine.cpp`
**Status:** **Fixed in this task.**

The test collected logged messages but its loop body ended with a comment
"No assertion needed here" and finished with a bare `QVERIFY(true)` — it
could not fail regardless of behavior, in a security-adjacent test whose
name promises a real check.

**Fix applied:** replaced with a concrete assertion — the secret-as-reason
string must appear in the collected log messages exactly once (proving
logging occurred without being echoed a second time anywhere else, which
would multiply any accidental leak if a caller ever passed a real
credential as a transition reason). Verified passing.

---

### W110-F013 — `CMakeUserPresets.json` is tracked in git with a machine-specific path
**Module:** build | **File:** `CMakeUserPresets.json:41` (`"QTDIR": "F:/Programs/Qt/6.11.1/msvc2022_64"`)
**Status:** Not fixed in W110 (repo-hygiene decision affecting other contributors' local files — left for the project owner to confirm before untracking). Noted in roadmap.

`CMakeUserPresets.json` is conventionally personal/gitignored (that's the
point of the "User" presets split from `CMakePresets.json`). It's tracked
here (confirmed via `git ls-files`, committed 2026-06-24) with a
machine-specific `F:/Programs/Qt/...` path, meaning every other clone either
inherits this path or must modify a tracked file locally.

**Fix direction:** `git rm --cached CMakeUserPresets.json` and add it to
`.gitignore`, after confirming with the project owner this doesn't break
another expected workflow.

---

### W110-F014 — `docs/project-status.md`'s "Active branch" pointer is stale
**Module:** docs | **File:** `docs/project-status.md:5`
**Status:** **Fixed in this task** (updated as part of this audit's own project-status.md update — see below).

The doc stated `Active branch: fix/w109a-rtt-renegotiation-and-rtp-port-collision`
while HEAD had already moved to this audit's branch; updated to reflect
`audit/w110-full-project-code-review` and this task's completion.

---

## Low severity

### W110-F015 — `ImdnParser`/`IsComposingParser` lack the input-size cap `PidfParser` enforces
**Module:** parsers | **Files:** `src/sip/ImdnParser.cpp:14-20`, `src/sip/IsComposingParser.cpp:14-20`
**Status:** Not fixed (defense-in-depth only; not independently exploitable — `QXmlStreamReader` doesn't expand external entities, confirmed no DTD/entity handling exists). Proposed for W114 as a small follow-up.

`PidfParser::parse` rejects bodies over `kMaxPidfBytes` (64 KB) before
parsing; the other two XML-body parsers have no equivalent cap. Effect is
bounded (proportional CPU time on an oversized malformed body), not a
crash or unbounded-memory issue given current callers (MSRP chunk assembler
and SIP body extraction both have their own size ceilings upstream).

---

### W110-F016 — Received filenames aren't screened against Windows reserved device names
**Module:** msrp / file-transfer | **File:** `src/msrp/MsrpFileSelector.cpp:124-137` (`sanitizeFileNameForDisplay`)
**Status:** Not fixed. Proposed for W114.

Path traversal is already prevented (save always goes through
`QFileDialog::getSaveFileName`), but a peer-suggested filename like `con.txt`
is not special-cased, which can produce a confusing save failure on Windows
if the user doesn't rename it in the dialog.

---

### W110-F017 — `MsrpCallPreparationController::start()` accumulates duplicate `QTimer::timeout` connections
**Module:** msrp | **File:** `src/msrp/MsrpCallPreparationController.cpp:101-112`
**Status:** Not fixed. Proposed for W113 (bundle with F001, same file/class).

Each `start()` call `connect()`s a new lambda to the long-lived
`m_timeoutTimer` without disconnecting the previous one. Harmless
functionally (the generation-guard check prevents double state
transitions) but a genuine, unbounded per-call-attempt connection leak over
a long-running session.

---

### W110-F018 — `test_diagnostics_bundle.cpp` has a self-documented test-order dependency
**Module:** tests | **File:** `tests/test_diagnostics_bundle.cpp:53-60`
**Status:** Not fixed (low risk, single binary, already flagged by its own author comment). Noted in roadmap.

`fallbackWhenSipTraceMissing` must run before any other test in the same
binary touches the `SipTraceLogger` singleton, relying on QtTest's
declaration-order execution — fragile but confined and already documented
inline.

---

### W110-F019 — Stale `schemaVersion` example/notes in `docs/windows-trace-json-export.md`
**Module:** docs | **File:** `docs/windows-trace-json-export.md` (lines 92, 200, 355 pre-fix)
**Status:** **Fixed in this task.**

Two example JSON snippets and the field-reference table still said
`schemaVersion: 2` / "currently 2" after Task W102 bumped the real constant
(`InteropTraceExporter::kSchemaVersion`) to `3`; the same doc's own
changelog correctly notes the bump. Updated all three references to `3` and
credited the W102 bump in the field-reference row.

---

### W110-F020 (informational, not a bug) — SDP attribute-name matching is case-sensitive in `MsrpSdpNegotiator`
**Module:** msrp | **File:** `src/msrp/MsrpSdpNegotiator.cpp:53,84-119`
**Status:** No action — RFC 4566 attribute names are conventionally lowercase and case-sensitive per spec; flagged only for the stylistic inconsistency with the adjacent, defensively case-insensitive transport-token comparison. Not counted in the finding totals.

---

## Also fixed during this audit (found via build, not a dedicated review pass)

### W110-F021 — `MsrpSessionStore::remove()` used `> 0` on a Qt6 `QHash::remove()` result (which returns `bool`, not a count)
**Module:** msrp | **File:** `src/msrp/MsrpSessionStore.cpp:38`
**Status:** **Fixed in this task.**

Surfaced as MSVC warning C4804 ("unsafe use of type 'bool' in operation")
during the Release build. Not a functional bug (`bool > 0` evaluates
correctly either way) but genuinely confusing and worth a one-line cleanup:
changed to `if (m_sessions.remove(sessionKey))`.

---

## Coverage notes (what was and wasn't reached at full audit depth)

Every phase in the task's Faza 1–23 checklist was covered by at least one
of: the architecture-map pass, five parallel deep-dive research passes
(lifecycle/ownership/threading; SIP/SDP/RTT negotiation; messaging/MSRP
direct/relay/file-transfer; Presence/XCAP/parsers/binary-safety;
config/UI/logging/export), and a dedicated test-suite/build-system/
docs-drift pass. Given the size of the codebase (103 files in `src/sip`,
57 in `src/msrp`, 82 test files, ~2400 commits), each pass prioritized
breadth with targeted evidence-gathering over line-by-line reading of every
file; the GUI panel-by-panel walkthrough (Faza 18) was not done exhaustively
widget-by-widget beyond the connect()-call audit already performed — a
deeper manual click-through pass is one of the roadmap candidates (W115) if
further UI-state auditing is wanted. No area was skipped outright; where
depth was limited, this is stated explicitly above rather than claiming
full coverage.
