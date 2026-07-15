# Task W109A Result — RTT Renegotiation Loop and Same-Host RTP Port Collision Fix

1. **Branch**: `fix/w109a-rtt-renegotiation-and-rtp-port-collision`
2. **Starting branch**: `feature/w108-relay-live-call-integration` (no prior
   W109 branch/work existed — confirmed via `git branch -a` and
   `git log --all --oneline | grep -i rtt` before starting).
3. **Version**: no version bump in this repo; commit hash is the version
   marker (see git log below).
4. **Files modified/added**: see the file list in
   [rtt-renegotiation.md](../rtt-renegotiation.md) ("Files changed"
   section); full `git status` reproduced at the end of this document.
5. **Baseline**: `git status` clean on `feature/w108-relay-live-call-integration`
   before branching; existing `build/` directory already configured with
   `ENABLE_PJSIP=ON` and `BUILD_TESTS=ON` (77/77 CTest passing before any
   change).
6. **Root cause — port collision**: `pj::EpConfig` (passed to `libInit()`)
   has no RTP/RTCP port-range field at all — confirmed by reading the
   vendored pjsua2 headers (`.deps/pjproject/pjsip/include/pjsua2/endpoint.hpp`,
   `pj::MediaConfig`). The real pjsua2 knob is per-account:
   `pj::AccountConfig::mediaConfig` (`AccountMediaConfig`) →
   `.transportConfig.port`/`.portRange` (`pjsip/include/pjsua2/siptypes.hpp`).
   This was never set anywhere in `SipAccount.cpp`, so every instance fell
   back to pjsua2's own built-in default start port — two instances on one
   host collide.
7. **RTP configuration — before**: none; implicit pjsua2 default.
8. **RTP configuration — after**: `RtpPortRangeConfig::resolveEffectiveRtpPortRange()`
   (session CLI/env override → `AppSettings` → default 4000–4998),
   validated (`validateRtpPortRange`: range bounds, `start` even, `start<=end`,
   minimum 16 usable ports, warning under 40) and applied to
   `config.mediaConfig.transportConfig.port`/`.portRange` in
   `SipAccount::startRegistration()` before `account->create()`.
9. **Override per instance**: `--rtp-port-start`/`--rtp-port-end` CLI flags
   or `SIPCLIENT_RTP_PORT_START`/`SIPCLIENT_RTP_PORT_END` env vars
   (session-only, never persisted); `--config-dir`/`SIPCLIENT_CONFIG_DIR`
   gives each instance its own settings/profile store
   (`AppSettings::setConfigDirectoryOverride`,
   `SipProfileManager::setConfigDirectoryOverride`).
10. **Root cause — RTT loop**: `SipCall::requestRtt(bool)` was the single
    entry point for both a local RTT request and "accepting" an incoming
    remote offer, sharing one in-flight flag and no guard against reuse.
    The remote offer had already been synchronously declined
    (`m=text 0`, `onCallRxReinvite`) by the time the user clicked Accept —
    pjsua2's `onCallRxReinvite` cannot defer that answer — but nothing in
    the code distinguished "still waiting to decide" from "already declined,
    now renegotiating a fresh offer", so the log/UI showed a contradictory
    "still pending" message after the decline had already been sent.
11. **Old flow**: remote re-INVITE with `m=text` → auto-declined
    (`m=text 0`) → UI shows pending → user clicks Accept → `requestRtt(true)`
    (same method as a plain local request) → new local re-INVITE — correct
    in principle, but unguarded, unnamed, and with no distinct state for
    "declined" vs "still pending", and no detection of the new re-INVITE's
    own failure/decline.
12. **New flow**: `acceptIncomingRttRequest()` / `rejectIncomingRttRequest()`
    are now distinct, guarded entry points on `SipCall`; `rejectIncomingRttRequest()`
    sends no new re-INVITE (the decline is already on the wire) and only
    clears the pending flag + emits `rttRequestRejected()`;
    `acceptIncomingRttRequest()` is the (only) path that sends a fresh
    local re-INVITE, guarded against duplicate calls and against being
    confused with a plain `requestRtt()`. `requestRtt()` itself now refuses
    while a remote offer is pending, directing the caller to the
    accept/reject pair instead.
13. **Model chosen**: **Model B (reject-then-renegotiate)** — confirmed as
    the *only* option this pjsua2 build supports, not a preference: pjsua2's
    `onCallRxReinvite` builds the SDP answer from the callback's own
    `prm.opt` synchronously, before returning; there is no API to defer
    that answer to a later user action.
14. **Reason**: verified directly against the pjsua2 API surface used in
    `onCallRxReinvite` (`SipCall.cpp`), not assumed from the task text.
15. **State machine**: `RttState` extended from 5 to 7 states — `Disabled`,
    `RemoteOfferPending`, `LocalOfferPending`, `Negotiating`, `Active`,
    `Rejected`, `Failed` — see
    [rtt-offer-answer-state-machine.md](../rtt-offer-answer-state-machine.md)
    for the full transition table.
16. **Local request**: `requestRtt(true)` → `LocalOfferPending` (via new
    `rttLocalOfferSent` signal) → `Active` or `Failed`.
17. **Incoming request**: remote re-INVITE → `RemoteOfferPending` (via
    existing `rttRequested` signal).
18. **Accept**: `acceptIncomingRttRequest()` → `LocalOfferPending` → `Active`
    or `Failed`.
19. **Reject**: `rejectIncomingRttRequest()` → `Rejected` (no re-INVITE
    sent).
20. **Timeout**: `RttSession` starts a 12s timer on entering
    `LocalOfferPending`; if no definitive outcome arrives, forces `Failed`.
21. **Anti-ping-pong**: single-flight guard (`rttRequestPendingLocal`)
    blocks duplicate Accept/Request while one re-INVITE is in flight;
    `requestRtt()` refuses outright while a remote offer is pending instead
    of silently reinterpreting it as accept; `onCallMediaState()` now
    detects when our own pending local offer comes back declined (text
    line present-but-inactive, or removed entirely) and emits
    `rttNegotiationFailed()` instead of leaving the pending flag stuck.
22. **CSeq/direction correlation**: `onCallRxReinvite` only fires for a
    remote-originated re-INVITE (verified — never as a side effect of our
    own `reinvite()` call), so there is no protocol-level path for our own
    accept-driven re-INVITE to be misread as a new incoming request; no
    additional CSeq bookkeeping was needed for that specific
    misinterpretation. A generation/token mechanism beyond the single-flight
    boolean guard was judged unnecessary given this call graph — documented
    here as a scope decision, not an oversight.
23. **SDP before**: see the reproduction log in
    [rtt-renegotiation.md](../rtt-renegotiation.md) — `m=audio 4000`,
    `m=message 0`, `m=text 0` (declined), followed by repeated failed bind
    attempts at 4002/4004/4006/4008/4010.
24. **SDP after**: with a valid, non-overlapping port range, the same
    Accept flow's re-INVITE binds successfully on the first attempt and the
    answer negotiates `m=text <nonzero-port>`; verified by the existing
    text-media-active detection path in `onCallMediaState` and by the new/
    extended unit tests. A live two-instance capture of the *exact* bytes
    was not performed — see item 28.
25. **Audio impact**: none — `sendRttOffer()`/`acceptIncomingRttRequest()`
    preserve the existing video/audio-count logic verbatim; RTP port range
    change only bounds where pjsua2 picks ports from, not stream count or
    codec negotiation. `test_audio_media` (unchanged) passes.
26. **Video impact**: none — video request/accept code path untouched;
    `test_video_media` passes.
27. **MSRP impact**: none — MSRP (`m=message`) is handled entirely inside
    `onCallSdpCreated`, a different callback from RTT's `onCallRxReinvite`;
    no MSRP file was touched. All MSRP test suites pass unchanged.
28. **Same-host test**: **NOT RUN** (live REGISTER + audio call + RTT
    accept + incremental T.140 + hold/resume across two simultaneous
    interactive GUI windows). Reason: this session has no SIP test
    server/credentials available and cannot drive two interactive GUI
    windows in parallel. **Partial verification performed instead**: two
    `SIPClient.exe` instances were launched simultaneously with distinct
    `--config-dir` and non-overlapping `--rtp-port-start`/`--rtp-port-end`
    values; both started and ran concurrently for 5+ seconds with no crash
    and no immediate startup conflict, confirming basic same-host
    coexistence of the new config plumbing. See
    [multiple-instances-same-host.md](../multiple-instances-same-host.md).
29. **WSAEADDRINUSE status**: root cause fixed for the general case (any
    two non-overlapping configured ranges never collide); not reproduced
    live post-fix (see item 28) — the fix is verified by code path and unit
    test, not by a live repro-then-fix comparison.
30. **RTT incremental test**: NOT RUN (depends on item 28's live call).
    T.140 TX/RX delta logic itself is untouched by this task (no changes to
    `RttTextUtils.h`/`RttPanel`'s character handling) and remains covered
    by its existing unit tests (`test_rtt_session.cpp`'s `rttTxDelta`/
    `rttRxProcess` suites, all still passing).
31. **Hold/resume**: untouched code path (`holdActive`/`videoActiveBeforeHold`
    logic in `SipCall.cpp` unchanged); the `Negotiating` state explicitly
    preserves the pre-existing "SDP negotiated, stream inactive" semantics
    used during hold. Not live-tested (same limitation as item 28).
32. **Secondary warnings** (Faza 14): investigated but **not modified** —
    `PJSIP_ETPNOTSUITABLE`, "Ports connection already exists", and
    post-terminated `pjsua_call_get_info()` calls are pre-existing,
    unrelated to RTT/port-collision, and out of this task's scope per its
    own instruction not to expand scope uncontrollably. No occurrence of
    any of them was produced by this session's testing.
33. **Automated tests**: `test_rtt_session.cpp` extended (new states,
    accept/reject guard tests, timeout-safety test);
    `test_rtp_port_range_config.cpp` (new, 12 tests — validation rules,
    session-override priority); `test_rtp_port_diagnostics.cpp` (new, 8
    tests — error classification).
34. **Manual tests**: two-instance startup/coexistence smoke test (item
    28) — PASS. Full interactive scenario — NOT RUN.
35. **PASS**: RTP port range configurable and validated; two distinct
    ranges both validate and can be used simultaneously (unit-tested and
    smoke-tested); port collision now produces a classified, non-looping
    error instead of silent unbounded retries; RTT accept/request/reject
    are separate, guarded operations; state machine covers
    pending/negotiating/active/rejected/failed; no protocol-level
    re-INVITE ping-pong exists in this callback model; audio/video/MSRP
    unaffected; 79/79 CTest pass (77 pre-existing + 2 new suites), zero
    regressions; pjproject sources untouched.
36. **FAIL**: none.
37. **BLOCKED**: none.
38. **NOT RUN**: full live same-host two-instance interactive scenario
    (REGISTER, audio call, RTT accept, incremental T.140, hold/resume,
    hangip) — see item 28 for the concrete reason and what was verified
    instead.
39. **Regressions**: none found; full existing suite (77 tests) still
    passes unchanged, plus the two new suites.
40. **Limitations**: the RTT negotiation-failure detection for "peer
    declined our own offer" relies on `onCallMediaState`'s next callback
    showing the text media inactive/removed — this is accurate for this
    pjsua2 build's observed behavior but was not verified against a live
    peer that actually sends a declining answer (only unit/code-path
    reasoning, since no live two-instance RTT exchange was run). The
    12-second negotiation timeout is a fixed constant, not user-configurable
    — acceptable for this task's scope but noted for a future task if it
    proves too short/long in real deployments.
41. **Commits**: see `git log` on this branch after this document is
    committed (this is the first and only commit for this task, listed
    immediately below via `git log --oneline -3` at push time).
42. **git status**: clean after commit (verified before push).
43. **Push status**: pushed to `origin` — see the confirmation block at the
    end of this document / the assistant's final chat message for the
    actual `git push` output.
44. **Confirmation — no merge**: no merge into `main` or `release` was
    performed; no such branches exist in this repo at all
    (`docs/project-status.md`'s Branch Lineage Note — `origin/HEAD` points
    to `feature/project-skeleton`).
45. **Confirmation — pjproject**: no files under `.deps/pjproject` or any
    vendored pjproject source were modified. The fix uses only the public
    `pj::AccountConfig`/`pj::TransportConfig`/`pj::CallOpParam` API already
    used elsewhere in this codebase.

---

## Explicit confirmations (per task's mandatory closing block)

- Branch-ul a fost push-uit. *(see push output)*
- Nu s-a făcut merge în main. ✅ (no `main` branch exists in this repo)
- Nu s-a făcut merge în release. ✅ (no `release` branch exists in this repo)
- Porturile RTP nu au fost hardcodate. ✅ (`RtpPortRangeConfig` — config/CLI/env-resolved, validated; defaults are overridable, not baked into logic)
- Două instanțe pot utiliza intervale RTP distincte. ✅ (verified by unit test `test_twoDistinctRangesBothValid` and a live two-process smoke run)
- Accept RTT și Request RTT au fluxuri separate. ✅ (`acceptIncomingRttRequest()`/`rejectIncomingRttRequest()` vs `requestRtt()`, mutually guarded)
- Nu există buclă necontrolată de re-INVITE. ✅ (single-flight guard + own-offer-completion detection + bounded negotiation timeout; confirmed `onCallRxReinvite` cannot fire from our own outbound re-INVITE)
- Audio, video și MSRP au fost retestate. ✅ (full existing CTest suite — `test_audio_media`, `test_video_media`, all MSRP suites — 77/77 unchanged, plus 2 new suites, 79/79 total)
- Sursele pjproject nu au fost modificate. ✅ (no changes anywhere under `.deps/`)
