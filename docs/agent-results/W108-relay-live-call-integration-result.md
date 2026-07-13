# Agent Result — Task W108: Async MSRP Relay Allocation and Live Call SDP Integration

## 1-3. Branch / starting point / versions

- Branch: `feature/w108-relay-live-call-integration`.
- Starting branch: `feature/w107-msrp-relay-authentication` (confirmed via
  `git branch -a`; pulled `--ff-only`, already up to date).
- No merge into `main`/`release` performed.
- JSON export schema: stays at 3 (purely additive — see item 4).

## 4. Schema version

Unchanged at 3. New top-level array `msrpCallPreparationEvents` (see
`src/sip/InteropTraceExporter.h/.cpp`) — every field is a preparation id,
resolved mode string, internal (non-sensitive) allocation id, and a
free-text reason that is never a credential — matches the pattern already
established for `msrpRelayEvents` (W107).

## 5. Files changed

**New:**
- `src/msrp/MsrpCallPreparation.h` — `MsrpCallTransportMode`,
  `PreparedMsrpOffer`, `makeDeleteLaterSharedPtr()`.
- `src/msrp/MsrpCallPreparationController.h/.cpp` — the async state machine.
- `src/msrp/MsrpCallPreparationDiagnosticsEvent.h`,
  `MsrpCallPreparationDiagnosticsStore.h/.cpp`.
- `tests/test_msrp_call_preparation.cpp` (6 cases).
- `docs/msrp-relay-call-preparation.md`, `msrp-relay-live-call-integration.md`,
  `msrp-relay-async-sdp-architecture.md`, `msrp-relay-call-lifecycle.md`,
  `msrp-relay-live-validation.md`, this prompt/result pair.

**Modified:**
- `src/core/AppSettings.h` — added `msrp/relay/*` config surface (mode,
  host, port, TLS, username, credential profile id, timeouts, refresh
  margin, max retries, preparation timeout), all defaulting to inert
  values (mode `"disabled"`).
- `src/msrp/MsrpRelayClient.h/.cpp` — added `takeTransport()` (ownership
  handoff to `MsrpSession`; see item 15/29).
- `src/sip/SipCall.h/.cpp` — `setPreparedMsrpOffer()`; `onCallSdpCreated`'s
  offerer branch consults the prepared offer before falling back to the
  unchanged direct-listener path; post-`makeCall()` transport adoption.
- `src/sip/SipManager.h/.cpp` — `dispatchMakeCall()`,
  `buildMsrpRelayConfigFromSettings()`, `cancelMsrpCallPreparation()`;
  `makeCall(uri)`/`makeCall(uri, opts)` now funnel through
  `dispatchMakeCall()`; `destroyActiveCall()` cancels any in-flight
  preparation; `makeEmergencyCall()` explicitly bypasses this (documented
  inline).
- `src/sip/InteropTraceExporter.h/.cpp` — additive `msrpCallPreparationEvents`.
- `tests/test_windows_trace_json_export.cpp` — redaction test for the new array.
- `CMakeLists.txt`, `tests/CMakeLists.txt` — new sources/test target wired in.
- `docs/project-status.md`, `docs/msrp-testing.md`, `docs/msrp-security.md`.

## 6. Baseline

`git status`/`branch`/`log -20` confirmed clean tree, correct starting
branch, W107's 8 commits present. Baseline build+ctest run *before any
W108 edit*: 74/74 (matches the W107 report exactly). One pre-existing,
unrelated build issue was found and worked around, not caused by W108: the
`live_registration_probe`/other `BUILD_LIVE_VALIDATION_TOOLS` targets fail
to compile `SipCall.cpp` standalone (a `PJMEDIA_HAS_VIDEO`-guarded helper
function ordering issue in that specific target's isolated compilation) —
these targets are not `ctest`-registered and were already broken before
this session touched anything; regression testing for this task used
`-DBUILD_LIVE_VALIDATION_TOOLS=OFF`, which builds and tests everything
`ctest` actually runs. This pre-existing issue is out of W108's scope and
was not investigated further.

## 7-9. Audit findings

- `SipManager::makeCall(uri)`/`makeCall(uri, CallMediaOptions)`/
  `makeEmergencyCall()` all funnel through `prepareOutgoingCall()` (creates
  `m_activeCall`, wires signals) then directly called
  `m_activeCall->makeCallWithOptions()` — fully synchronous, no seam for
  async work existed before W108.
- `SipCall::makeCallWithOptions()` calls `pj::Call::makeCall()`
  synchronously; this triggers `onCallSdpCreated` on the same call stack
  before `makeCall()` returns (confirmed by reading the pjsua2-generated
  SDP flow, not assumed).
- `onCallSdpCreated`'s offerer branch (`isOfferer == true`, i.e.
  `prm.remSdp.wholeSdp.empty()`) calls `m_impl->startMsrpPassiveListener()`
  — a synchronous `QTcpServer::listen()`, non-blocking but definitely
  local-only I/O, then builds the SDP `m=message` section from
  `m_impl->msrpLocalUri`.
- Threading: `onCallSdpCreated` runs on whatever thread pjsua2 invokes call
  callbacks on; existing code already treats it as "PJSIP thread, must not
  block, must not touch UI" (see the extensive comments already in
  `SipCall.cpp` about `Qt::QueuedConnection` elsewhere in the same file).

## 10-11. Call preparation architecture / state machine

See [msrp-relay-call-preparation.md](../msrp-relay-call-preparation.md) for
the full state diagram and rationale.

## 12-13. Cancellation / race protection

Generation-token mechanism in `MsrpCallPreparationController`; unit-tested
directly (`cancelDuringAllocationSuppressesLateSignals`). Full enumeration
of the spec's race list, including one **documented, not fully guarded**
edge case (starting a second call while the first is still mid-preparation)
— see [msrp-relay-call-lifecycle.md](../msrp-relay-call-lifecycle.md),
"Cancellation / races".

## 14-15. Prepared SDP context / ownership

`PreparedMsrpOffer` (`src/msrp/MsrpCallPreparation.h`) — see that file's
comments and [msrp-relay-call-preparation.md](../msrp-relay-call-preparation.md),
"Ownership", for the full ownership table (who creates/owns/transfers/
closes/destroys `MsrpRelayClient`/`MsrpTransport`/`MsrpRelayAllocation`/
`MsrpSession`/`SipCall`).

## 16-21. Outbound / inbound / relay policy / direct MSRP

- **Outbound**: implemented for all three modes (Disabled/Automatic/
  Required) — see [msrp-relay-live-call-integration.md](../msrp-relay-live-call-integration.md),
  "Outbound calls".
- **Inbound**: **NOT IMPLEMENTED / BLOCKED**, with the specific
  architectural reason documented (no safe, verified way found this
  session to defer pjsua2's inbound-answer SDP construction) — see that
  same doc, "Inbound calls — NOT IMPLEMENTED". This is the single largest
  scope gap in this task; recommended as the first item for a follow-up.
- **Direct MSRP**: unaffected when relay is disabled/unconfigured (the
  shipped default) — `onCallSdpCreated` takes the exact pre-W108 code path.
  Regression-verified via the full CTest suite; **not live-retested with a
  real peer this session** (no live environment available — see
  [msrp-relay-live-validation.md](../msrp-relay-live-validation.md)).

## 22-25. Allocation timing / SDP injection / wire evidence

Allocation is fully resolved (or definitively failed/timed out) before
`SipManager::dispatchMakeCall()` calls `SipCall::makeCallWithOptions()`
— by construction, not by inference (the call happens from inside the
controller's `ready`/`failed` signal handlers). **No wire-bytes evidence
of a relay path in a real INVITE/answer exists** — this requires a real
relay allocation, which requires live credentials this session did not
have (item 49/54).

## 26-28. Audio / video / RTT impact

None — the entire W108 code path is gated behind
`AppSettings::msrpRelayMode()` defaulting to `"disabled"`; when disabled,
`dispatchMakeCall()`'s first branch calls
`m_activeCall->makeCallWithOptions()` directly, identical to the
pre-W108 code. Regression-verified (item 60).

## 29-32. Transport adoption / mapping

`MsrpRelayClient::takeTransport()` (new) hands the relay's already-
connected `MsrpTransport` to `MsrpSession::adoptExternalTransport()` (W107)
right after `pj::Call::makeCall()` returns in `SipCall::makeCallWithOptions()`.
Mapping: allocation ↔ Call-ID is implicit (the `PreparedMsrpOffer` was
resolved specifically for this `SipCall` instance, one-to-one, before the
call existed); allocation ↔ media index follows the existing W101
`setMediaIndex()` call already present in the offerer branch; allocation ↔
`MsrpSession` is direct (same object). **Not verified against a real relay
or real call** — see item 49.

## 33-37. SEND / response / REPORT / IMDN

Not exercised this session (requires a live relay-bound call). The
transport-adoption *mechanism* itself is unit-tested at the `MsrpSession`
level by W107's `test_msrp_session_relay_transport`, which is unmodified
and still passes.

## 38-42. Fallback / duplicate prevention / refresh / reallocation / re-INVITE

- Fallback policy and duplicate-transport prevention: implemented and
  unit-tested (`relayAutomaticFailureFallsBackToDirect`,
  `relayRequiredFailureEmitsFailed`) — see
  [msrp-relay-call-lifecycle.md](../msrp-relay-call-lifecycle.md).
- Refresh during an active call and reallocation/re-INVITE: **NOT
  IMPLEMENTED**, documented in detail (with the mechanism explaining
  exactly what happens instead — a silent new allocation nothing reacts
  to) in the same doc.

## 43. Cleanup

`shared_ptr` with a `deleteLater()` deleter throughout — no synchronous
delete of a `QObject` from inside its own signal handler anywhere in the
new code (mirrors the pattern W107 already established for
`MsrpRelayClient` itself, after finding a real segfault there).

## 44-45. W107 authenticated-AUTH investigation / RFC diff

Static code review only, no live capture available this session — see
[msrp-relay-live-validation.md](../msrp-relay-live-validation.md), "Phase
15". No fix applied (correctly — the task's own rule against modifying the
digest based on assumptions was followed; nothing in the review produced
concrete, actionable evidence of a specific bug).

## 46-48. UI / diagnostics / export

- UI: **not implemented this session** — there is no settings UI for the
  new `msrp/relay/*` config surface (same as W107's own relay config,
  which also has no UI). Config is INI/test-only.
- Diagnostics: `MsrpCallPreparationDiagnosticsStore` records every state
  transition (resolving/allocating/ready/failed/cancelled).
- Export: additive `msrpCallPreparationEvents` array, schemaVersion stays
  3, redaction verified by a new test
  (`exportMsrpCallPreparationEventsFieldsRedacted`).

## 49-51. Comparator / live tests

- Comparator (`TraceComparator`) extension: **deferred**, same reason as
  W107 — no SIP-Server-RTT trace available to correlate against.
- SIP2SIP live test / direct MSRP live test / audio-video-RTT live test:
  see [msrp-relay-live-validation.md](../msrp-relay-live-validation.md) for
  the full per-scenario table.

## 52-56. Status summary (strict vocabulary)

- **PASS**: automated regression suite (75/75, item 60); RFC 2617 digest
  vectors (unchanged from W107); `MsrpCallPreparationController` unit
  tests (6/6).
- **FAIL**: none newly introduced. (W107's already-recorded relay-allocation
  FAIL against sip2sip.info remains FAIL, not re-tested this session.)
- **BLOCKED**: inbound relay integration (architectural); all live
  sip2sip.info scenarios (no credentials).
- **NOT RUN**: direct-MSRP/audio/video/RTT live retest with a real GUI/peer
  (no interactive environment); SEND/response/REPORT through a real
  relay-bound call; comparator extension.
- **UNSUPPORTED**: not applicable — nothing was found to be structurally
  unsupported by the relay protocol itself.

## 57-59. Problems found

- **Client**: the pre-existing `live_registration_probe`/friends build
  issue described in item 6 (unrelated to W108, worked around for
  regression testing, not fixed).
- **Relay**: unchanged from W107 — authenticated AUTH still unconfirmed
  (no new evidence gathered this session).
- **Server (SIP-Server-RTT)**: not applicable — not available this session.

## 60-62. Automated tests / manual tests / regressions

- Automated: 75/75 CTest targets pass (74 pre-existing W090–W107 + 1 new
  `test_msrp_call_preparation`), zero regressions, verified by running the
  full suite before (74/74) and after (75/75) every source change in this
  session.
- Manual: none performed (no live environment).
- Regressions: none found.

## 63-65. Limitations / risks / W109 candidates

See the "NOT IMPLEMENTED" sections throughout
[msrp-relay-live-call-integration.md](../msrp-relay-live-call-integration.md)
and [msrp-relay-call-lifecycle.md](../msrp-relay-call-lifecycle.md).
Concretely, for a future task:

1. Inbound relay-offer answering (currently BLOCKED — needs a pjsua2
   answer-timing investigation).
2. Reacting to `allocationRefreshed()` mid-call with a re-INVITE/UPDATE
   carrying the new Use-Path (currently the refreshed allocation is
   silently unused).
3. The second-call-during-preparation race (documented gap, no automated
   test).
4. A settings UI for `msrp/relay/*` config (still INI/test-only).
5. Live validation end-to-end once relay credentials are available again
   (Phase 15's digest investigation needs a raw packet capture to make
   further progress).

## 66-68. Commits / git status / push status

See the git log after this report; commits are thematic (preparation
controller, SipCall/SipManager wiring, diagnostics/export, tests, docs).
`git status --short` after committing shows only pre-existing untracked
scratch `.bat`/`.log` files (never committed, per established repo
convention). Pushed to `origin/feature/w108-relay-live-call-integration`.

## 69-70. No-merge / pjproject confirmation

See the mandatory confirmation block below.

---

## Confirmare Finală Obligatorie

Branch-ul `feature/w108-relay-live-call-integration` a fost push-uit.
Nu s-a făcut merge în `main`.
Nu s-a făcut merge în `release`.
Relay allocation nu este executată în callback-ul sincron `onCallSdpCreated`
— alocarea are loc integral în `MsrpCallPreparationController`, înainte ca
`SipCall::makeCallWithOptions()` să fie apelat.
Callback-ul SDP nu efectuează I/O și nu blochează — citește doar
`PreparedMsrpOffer`, deja rezolvat.
MSRP direct a fost păstrat neschimbat (verificat structural — `onCallSdpCreated`
urmează exact codul pre-W108 când relay este dezactivat) și retestat prin
suita automată completă (75/75); nu a fost retestat live cu un peer real în
această sesiune (mediu indisponibil).
Audio și video și RTT au fost retestate prin suita automată completă
(75/75, zero regresii); nu au fost retestate live într-un apel GUI real în
această sesiune (mediu indisponibil).
Nicio credențială nu a fost salvată în repository — nicio credențială de
relay nu a fost nici măcar folosită în această sesiune (indisponibilă).
Niciun rezultat relay live nu a fost declarat PASS fără allocation, SDP
wire real și dialog real — toate scenariile live relevante sunt marcate
BLOCKED sau NOT RUN, nu PASS.
Sursele pjproject nu au fost modificate — toate schimbările sunt în cod
propriu al aplicației (`src/msrp`, `src/sip`), folosind API-ul pjsua2
existent, neschimbat.
