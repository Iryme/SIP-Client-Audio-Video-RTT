# Agent Result — Task W107: MSRP Relay Authentication (RFC 4976)

## 1. Branch

`feature/w107-msrp-relay-authentication`, branched from
`feature/w106-msrp-connection-resilience` (the real W106 branch name — the
task text's literal `feature/w106-msrp-connection-hijack-dos-recovery` does
not exist in this repo). Not merged into `main`/`release`/any other branch.

## 2. Version / schema

`InteropTraceExporter::kSchemaVersion` stays **3** — the new
`msrpRelayEvents` array is purely additive, same convention as W100/W102.

## 3. Files changed

### Added

- `src/msrp/MsrpDigestAuth.h/.cpp` — RFC 2617 HA1/HA2/response + challenge
  parsing, from scratch (no reusable primitive existed anywhere in the
  repo — confirmed by an explicit audit before writing any code).
- `src/msrp/MsrpRelayConfig.h` — relay mode/host/port/transport/credential-
  source/TLS/timeout/retry configuration model.
- `src/msrp/MsrpRelayAllocation.h` — `Use-Path`/expiry result model.
- `src/msrp/MsrpRelayDiagnosticsEvent.h`,
  `src/msrp/MsrpRelayDiagnosticsStore.h/.cpp` — mirrors the existing
  `MsrpDiagnosticsEvent`/Store pattern.
- `src/msrp/MsrpRelayClient.h/.cpp` — the AUTH/challenge/allocate/refresh/
  reconnect/close state machine.
- `tests/test_msrp_digest_auth.cpp`, `tests/test_msrp_relay_client.cpp`,
  `tests/test_msrp_session_relay_transport.cpp` — new test targets.
- `tests/live_msrp_relay_probe.cpp` — manual live-validation tool (pure
  Qt/Network, no PJSIP dependency, built only under
  `BUILD_LIVE_VALIDATION_TOOLS=ON`).
- `docs/msrp-relay-authentication.md`, `docs/msrp-relay-allocation.md`,
  `docs/msrp-relay-security.md`, `docs/msrp-relay-testing.md`,
  `docs/msrp-relay-sip2sip-validation.md`.
- `docs/agent-prompts/W107-msrp-relay-authentication.md`,
  `docs/agent-results/W107-msrp-relay-authentication-result.md` (this file).

### Modified

- `src/msrp/MsrpSession.h/.cpp` — new `adoptExternalTransport()`: adopts an
  already-connected transport instead of opening one via
  `connectAsActive()`/`listenAsPassive()` (the primitive relay-assisted
  MSRP needs to reuse a relay's control connection as the session's media
  connection).
- `src/sip/InteropTraceExporter.h/.cpp` — additive `msrpRelayEvents` array,
  new `msrpRelayEvents` overload parameter.
- `src/gui/panels/MsrpPage.h/.cpp` — new "MSRP Relay Diagnostics (RFC 4976,
  Experimental)" read-only panel, wired to `MsrpRelayDiagnosticsStore`.
- `CMakeLists.txt` (root) and `tests/CMakeLists.txt` — new source/test
  target wiring (`MSRP_RELAY_SOURCES`, 3 new test targets, 1 new live probe
  target).
- `docs/msrp-security.md`, `docs/msrp-testing.md` — pointer sections to the
  new relay docs.
- `docs/project-status.md` — task table + module status row.

## 4. Standard and AUTH-format source

RFC 4976 "Relay Extensions for the Message Sessions Relay Protocol
(MSRP)": new `AUTH` method, HTTP-Digest-style (RFC 2617) challenge/response
with `method="AUTH"` and `digest-uri` = the relay's own MSRP URI,
`Use-Path`/`Expires` on success. No prior relay code, fixture, or design
note existed anywhere in this repo before this task — confirmed via a
repo-wide search in Phase 1 (only hit: W102's explicit out-of-scope note
and a companion-server informational mention). See
[msrp-relay-authentication.md](../msrp-relay-authentication.md) for full
detail.

## 5. Architecture

`MsrpRelayClient` — a separate component, not folded into `MsrpSession`.
13-state machine (`Idle/Connecting/Connected/SendingInitialAuth/
WaitingChallenge/SendingAuthenticatedAuth/WaitingAllocation/Allocated/
Refreshing/Reauthenticating/Closing/Closed/Failed`, `Q_ENUM`). Reuses
`MsrpTcpTransport`/`MsrpTlsTransport` unmodified (a relay control
connection is structurally just an outbound MSRP-framed TCP/TLS
connection). Full detail in
[msrp-relay-authentication.md](../msrp-relay-authentication.md).

## 6. Configuration / credential source

`MsrpRelayConfig` — `mode` defaults `Disabled`; every other field
(host/port/transport/username/TLS policy/timeouts) defaults empty/zero.
`MsrpRelayClient::configure(config, password)` takes the password directly
(loaded by the *caller* from `CredentialStore`, exactly like
`SipAccount`/`XcapClient` already do — the relay client itself never
touches `CredentialStore`). No literal host/port/credential anywhere in the
new code; see [msrp-relay-security.md](../msrp-relay-security.md).

## 7–20. AUTH flow, digest, qop, nonce, stale nonce, authenticated AUTH, allocation, Use-Path, expiry, refresh, cleanup

All covered in detail in
[msrp-relay-authentication.md](../msrp-relay-authentication.md) and
[msrp-relay-allocation.md](../msrp-relay-allocation.md). Summary:
RFC-2617-standard HA1/HA2/response (verified against RFC 2617's own worked
example), qop-present and qop-absent both handled, nonce/cnonce/nc tracked
per client instance, a `401` while `WaitingAllocation` (rejecting the
*authenticated* retry) is a hard failure counted against `maxRetries` — not
an infinite re-challenge loop (a real bug found via testing, fixed).
`Use-Path`/`Expires` parsed via the existing `MsrpPath::parsePath()`;
refresh re-drives the full challenge/response cycle over the same
connection before `refreshMarginSeconds` (config) before expiry; cleanup on
`close()` stops both timers and gracefully closes the transport.

## 21. TLS

Reuses `MsrpTlsTransport` unmodified; `tlsVerifyPeer` defaults `true`,
custom CA only when explicitly configured — no new TLS code, no
verification bypass introduced.

## 22. SDP integration — NOT done, explicitly deferred to W108

`MsrpSipMediaInjector::injectMessageMedia()` already accepts an arbitrary
`localUri` parameter from its caller, so the *mechanism* to offer a relay
path exists unchanged. What does **not** exist: any caller in `SipCall`
obtaining a relay allocation *before* `onCallSdpCreated` fires and passing
its URI in instead of the local listener's. This is a deliberate,
documented scope decision, not an oversight — see
[msrp-relay-authentication.md](../msrp-relay-authentication.md)'s "What is
deliberately not wired" section for the full reasoning (synchronous pjsip
callback vs. inherently-asynchronous relay allocation; the restructuring
needed touches `SipManager::makeCall`, which is audio/video/RTT-call-
critical code this task's own rules forbid breaking, and which cannot be
safely validated without live GUI testing in this session). **Consequence:
relay path bytes never appear in any real SIP wire message in this build.**
Item 25 of the requested 64-item checklist ("dovada relay path în SIP wire
bytes") is therefore explicitly **NOT MET** — reported here rather than
worked around with an untested, risky change.

## 23. Mapping dialog ↔ allocation ↔ session

`MsrpRelayClient::setCorrelation(sipCallIdRedacted, mediaIndex)` exists so
diagnostics can be tagged per-call; actual per-call `MsrpRelayClient`
ownership (needed for two simultaneous calls to have independent
allocations) is not wired for the same reason as §22 — no live caller
exists yet.

## 24–33. SEND/response/REPORT via relay, CPIM/IMDN/is-composing, file transfer, direct compatibility, fallback, recovery, duplicate prevention

`MsrpSession::adoptExternalTransport()` is the tested primitive that would
carry ordinary SEND/response/REPORT (and therefore CPIM/IMDN/
is-composing/file-transfer, which are payload-layer, transport-agnostic)
over a relay-adopted connection exactly like any other session — verified
by `test_msrp_session_relay_transport`. It is not exercised end-to-end
through a live relay in this task (blocked by §22's deferral). Direct-MSRP
compatibility is provably unaffected: relay mode defaults `Disabled`, and
no existing code path calls into any relay class (see
[msrp-relay-testing.md](../msrp-relay-testing.md)). No duplicate-send logic
was needed since no caller routes through both paths simultaneously.

## 34. UI diagnostics

New "MSRP Relay Diagnostics (RFC 4976, Experimental)" table on `MsrpPage`,
read-only, redacted fields only (kind/response/algorithm/allocated-path-
redacted/expiry/error — never nonce/digest/credentials), a "Clear" button.
Empty by default since relay mode is off and nothing populates it in a real
call yet.

## 35. Export JSON

`msrpRelayEvents` array added additively to `InteropTraceExporter`;
`schemaVersion` stays 3. Structurally redacted — see §36.

## 36. Comparator

**NOT extended.** `TraceComparator`'s relay-specific correlation (Phase 15
of the task) was not implemented — there is no SIP-Server-RTT relay trace
available in this session to correlate against, and adding untested
correlation fields for a format that cannot be verified against real
server output was judged lower value than the client-side work actually
completed and verified. Documented gap for W108.

## 37. Test sip2sip

**Executed.** See
[msrp-relay-sip2sip-validation.md](../msrp-relay-sip2sip-validation.md).
Summary: relay auto-discovered via DNS SRV
(`msrprelay.sipthor.net:2855`); TLS connect **PASS**; initial `AUTH` /
`401` challenge (real `WWW-Authenticate: Digest realm=..., nonce=...,
algorithm=MD5, qop="auth"` from the live relay) **PASS**; authenticated
`AUTH` retry **FAIL** — the relay accepted the connection and the first
request but never responded to the second (digest) request across three
independent reconnect attempts (8s timeout each). Allocation never
obtained. Reported as **FAIL**, not fabricated as PASS — no `Use-Path` was
ever received.

## 38. Test SIP-Server-RTT

**NOT RUN.** Separate repository, not present/available in this session's
workspace; no local relay harness for it existed to substitute.

## 39–45. Raw evidence

Raw SIP bytes: not applicable (§22 — no SDP integration exists to produce
any). Raw MSRP relay-control bytes: captured indirectly via
`MsrpRelayDiagnosticsStore`'s redacted event log, reproduced verbatim in
[msrp-relay-sip2sip-validation.md](../msrp-relay-sip2sip-validation.md)
(kind/responseCode/algorithm/qopUsed/error columns for every AUTH attempt).
No packet-capture tool was available in this environment; the diagnostics
store's own redacted log is the retained evidence.

## 46–50. PASS / FAIL / BLOCKED / NOT RUN / UNSUPPORTED summary

| Item | Result |
|---|---|
| Digest primitives (RFC 2617 worked example) | PASS |
| Relay AUTH state machine (local scripted relay) | PASS |
| Refresh cycle (local scripted relay) | PASS |
| Retry/failure bound on rejected auth | PASS (bug found + fixed) |
| `adoptExternalTransport` (local loopback) | PASS |
| JSON export additive + redacted | PASS |
| Direct MSRP regression (71 pre-existing tests) | PASS, zero regressions |
| Live relay: DNS discovery | PASS |
| Live relay: TLS connect | PASS |
| Live relay: initial AUTH / 401 challenge | PASS |
| Live relay: authenticated AUTH / allocation | **FAIL** |
| Live relay: SEND through relay | BLOCKED (no allocation) |
| SDP live-wiring (relay path in real INVITE bytes) | **NOT DONE** (deliberately deferred, §22) |
| SIP-Server-RTT relay test | NOT RUN (repo unavailable) |
| Comparator relay correlation | NOT DONE (deferred, §36) |

## 51–53. Client / relay / server problems

- **Client problem, found and fixed**: infinite reconnect loop on a
  rejected authenticated AUTH (would have hung forever against a
  wrong-password/misbehaving relay).
- **Client problem, found and fixed**: segfault destroying a transport from
  inside its own synchronous signal handler during reconnect.
- **Relay problem (or client/relay incompatibility — not fully
  diagnosable client-side)**: msrprelay.sipthor.net silently drops the
  authenticated AUTH retry rather than responding with success or an error
  code. Three hypotheses recorded in
  [msrp-relay-sip2sip-validation.md](../msrp-relay-sip2sip-validation.md)
  §"Interpretation" — none confirmed.

## 54–55. Automated / manual tests

Automated: 74/74 CTest (71 pre-existing + `test_msrp_digest_auth` +
`test_msrp_relay_client` + `test_msrp_session_relay_transport`), zero
regressions. Manual: live sip2sip.info relay run, see §37.

## 56. Regressions

None. 74/74 passing.

## 57. Limitations

- SDP/call-flow integration not done (§22) — relay mode has no effect on
  any real call.
- Comparator relay correlation not done (§36).
- Several of the ~15 existing docs the full task instructions list for
  update (`msrp-foundation.md`, `msrp-protocol.md`, `msrp-transport.md`,
  `msrp-live-sdp-integration.md`, `msrp-peer-association.md`,
  `msrp-fallback.md`, `msrp-live-interoperability.md`,
  `msrp-file-transfer.md`, `windows-trace-json-export.md`,
  `client-server-trace-comparison.md`, `sip2sip-live-validation.md`) were
  **not** individually touched — none of their described behavior changed
  (relay is fully additive and inert), so a pointer was added only to
  `msrp-security.md` and `msrp-testing.md`, the two most directly relevant.
  `sip2sip-live-validation.md` itself does not exist on this branch (it was
  written in a later, different-branch session not merged into this
  lineage) — the new `msrp-relay-sip2sip-validation.md` cross-references it
  by name for when the branches are eventually reconciled.
- Live relay allocation itself did not succeed (§37/§46) — relay-assisted
  MSRP is therefore unverified end-to-end against any real relay.

## 58. Security risks remaining

- None new beyond what's documented in
  [msrp-relay-security.md](../msrp-relay-security.md)'s existing
  "Experimental status" section — since relay mode is inert by default and
  unreachable from any real call, its risk surface is currently limited to
  the manual live-probe tool and the (empty-by-default) UI panel.

## 59. What remains for W108

1. Restructure `SipManager::makeCall`/`SipCall` call-initiation ordering to
   pre-allocate a relay session (async) before `onCallSdpCreated` (sync)
   fires, then use `adoptExternalTransport()` to route the actual session
   over the relay's connection — validated live, with real SIP wire bytes
   captured.
2. Diagnose why msrprelay.sipthor.net silently drops the authenticated
   AUTH retry — ideally with a packet capture or a second known-working
   client's exchange to compare against.
3. Comparator relay-event correlation (Phase 15), once a real
   SIP-Server-RTT relay trace is available to validate against.
4. Two-simultaneous-calls / two-simultaneous-allocations mapping test, once
   §1 exists to test it through.

## 60. Commits

Thematic commits on this branch (see
`git log feature/w107-msrp-relay-authentication`):

1. `feat(msrp): add RFC 2617 digest primitives for relay AUTH`
2. `feat(msrp): add MSRP relay client, config, allocation, and diagnostics`
3. `fix(msrp): bound relay AUTH retries and defer reconnect to avoid mid-signal transport destruction`
4. `feat(msrp): add adoptExternalTransport for relay-assisted session reuse`
5. `feat(sip): add msrpRelayEvents to interop JSON export`
6. `feat(gui): add experimental MSRP relay diagnostics panel`
7. `test(msrp): add digest, relay client, and adopted-transport coverage`
8. `test(sip): cover msrpRelayEvents export redaction`
9. `docs(msrp): document RFC 4976 relay authentication, allocation, security, testing, and sip2sip live validation`

(Exact hashes visible via `git log` on this branch.)

## 61. git status (post-work)

Clean except pre-existing untracked `.bat` helper scripts (same convention
as every prior task — not committed).

## 62. Push status

Pushed to `origin/feature/w107-msrp-relay-authentication`.

## 63. Confirmation: no merge

Confirmed — no merge into `main`/`release` performed.

## 64. pjproject sources

Not modified. MSRP (direct and relay) remains a standalone Qt/C++ protocol
stack with zero dependency on vendored pjproject sources.

## Final confirmation block

```
Branch-ul feature/w107-msrp-relay-authentication a fost push-uit.
Nu s-a făcut merge în main.
Nu s-a făcut merge în release.
MSRP direct a fost păstrat și retestat (74/74 CTest, zero regresii).
Nicio credențială de test nu a fost salvată în repository.
Niciun rezultat de interoperabilitate relay nu a fost declarat PASS fără test live și dovezi
  — testul live cu sip2sip.info a confirmat conectarea TLS și primul challenge AUTH real,
  dar alocarea finală a EȘUAT (relay-ul nu a răspuns la AUTH autentificat) și este raportată FAIL.
Sursele pjproject nu au fost modificate.
Integrarea SDP live (oferirea path-ului relay într-un apel real) NU a fost făcută în acest task —
  motiv documentat explicit în raport, amânat pentru W108.
```
