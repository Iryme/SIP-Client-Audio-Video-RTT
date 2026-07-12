# Task-W102 — MSRP Live Interoperability (agent prompt, condensed)

## Repository / Branches

`SIP-Client-Audio-Video-RTT`. Branch chain (must be executed strictly in
order): `feature/w101-live-msrp-sip-integration` → **`feature/w102-msrp-live-interoperability`**
→ `feature/w103-lmpe-foundation` → `feature/w104-msrp-file-transfer` →
`feature/w105-advanced-msrp-hardening`. No merge into main/release/other
integration branches without explicit request.

## Scope

Validate and correct the W100/W101 MSRP implementation using real SIP
dialogs and MSRP connections. Must resolve before LMPE (W103): responding
to peer-initiated MSRP offers; incoming-connection association; MSRP role
validation; offer/answer behavior; full diagnostics UI wiring; export JSON
v3; client/server comparator; end-to-end SIP-Server-RTT test;
Blink/AG Projects test if the environment is available. LMPE itself is
explicitly out of scope for W102.

## Starting state (from W101)

Live `m=message` SDP injection (offerer path only), per-call MSRP listener
started before the offer, initial SIP↔MSRP dialog mapping, transport
policy gating the composer, SEND→response/REPORT feeding history/
diagnostics, 67/67 tests. Declared W101 limitations: no answer for
peer-initiated offers, no incoming-connection validation, incomplete UI
diagnostics, no schemaVersion 3, live interoperability `BLOCKED`/`NOT RUN`.

## Phases (13)

1. Baseline audit (build + full test suite before any change).
2. Offer/answer completeness: client-offers-MSRP combinations (peer
   accepts/rejects-via-port-0/changes role/omits m=message/restricts
   accept-types), peer-offers-MSRP combinations (client answers, listener
   started before answer, accept/reject-via-port-0, other media unaffected),
   renegotiation (re-INVITE/UPDATE local+remote, hold/resume, role change,
   remove/reintroduce MSRP, multiple m=message sections) — rejecting MSRP
   must never reject the whole call.
3. Setup roles: active/passive/actpass validated end-to-end; prevent
   active/active and passive/passive; clear diagnostic on role conflict.
4. Incoming peer association: never IP/port/URI alone; use Call-ID, tags,
   media index, session-ids, To-Path/From-Path, role, transport, connection
   ID, generation, timestamp/expiry; explicit handling for no-match/
   exact-match/multiple-candidates/ambiguous/stale/path-mismatch/
   role-mismatch/transport-mismatch/already-bound/reconnect/duplicate
   socket/delayed connection/socket-after-dialog-end; ambiguous match must
   never be arbitrarily resolved.
5. Simultaneous sessions to the same peer (2–3 calls, multiple m= sections,
   independent reconnect/close).
6. Early dialogs & forking: protect mapping across 180/183/PRACK/UPDATE/
   CANCEL/non-2xx/forked branches/two 2xx; abandoned-branch MSRP must never
   attach to the winning dialog; if the stack can't be driven through real
   forking, add fixtures and report live as NOT RUN.
7. TLS: msrp:// and msrps://, configurable certs, peer-name validation, CA
   trust, explicit self-signed test opt-in (never a silent default), no
   secret-exposing error text, configurable timeouts.
8. Transport policy & recovery: all four modes, MSRP-available/unavailable
   behavior per mode, socket-failure → fallback → reconnect → recovery flow
   with no permanent fallback and no duplicate retransmission across two
   transports.
9. UI diagnostics: selected/negotiated/actual transport, fallback
   used/reason, dialog/negotiation/listener/socket/TLS state, local/remote
   role, redacted paths, connection ID, peer-association result/confidence,
   MSRP transaction/response/REPORT status, IMDN status, last error,
   reconnect count, session expiry — never updated directly from a
   PJSIP/socket callback.
10. Export JSON schemaVersion 3 (additive): transportDecision, msrpSessions,
    msrpTransactions, peerAssociation, fallbackEvents, interopValidation,
    negotiationState, tlsDiagnostics; redact paths/tokens/certs/credentials;
    compatibility tests (v2 readable, v3 exportable, new fields absent
    safely in old data).
11. Client/server comparator correlating Call-ID/tags/session-id/
    transaction-id/Message-ID/IMDN-ID/direction/timestamp tolerance/SEND/
    response/REPORT/content-type/fallback/association/transport; detects
    missing/duplicate/direction-mismatch/id-mismatch/timing/payload-type/
    unexpected-fallback/mapping-mismatch/orphan-response/orphan-report/
    inconsistent-session-id; deterministic, machine-readable.
12. SIP-Server-RTT live test (14 minimum scenarios) if the environment is
    available; capture raw SIP/TCP/TLS, client export, server export/log,
    comparator result; server problems documented separately, never masked
    in the client.
13. Blink/AG Projects (if available) and Linphone (only for functionality
    it actually supports — never claim MSRP interop without it).

## Automated tests

SDP (peer-offer/local-answer/port-0/re-INVITE/UPDATE/hold-resume/multiple
m=message/accept-types/setup-roles), peer association (exact/no-match/
ambiguous/stale/path-mismatch/role-mismatch/transport-mismatch/duplicate/
delayed/multi-call), transport (available/fallback/recovery/required/
no-duplicate/socket-failure/reconnect), TLS (valid/invalid-CA/hostname-
mismatch/self-signed-test/timeout), export/comparator (v2 compat, v3,
missing/duplicate/direction/timing/id/association mismatch), full W090–W101
regression.

## Documentation

New: `docs/msrp-offer-answer.md`, `docs/msrp-live-interoperability.md`,
`docs/msrp-tls.md`, `docs/msrp-early-dialogs-and-forking.md`,
`docs/client-server-trace-comparison.md`. Updated: `docs/msrp-live-sdp-integration.md`,
`docs/msrp-peer-association.md`, `docs/msrp-fallback.md`,
`docs/msrp-foundation.md`, `docs/msrp-transport.md`, `docs/msrp-protocol.md`,
`docs/msrp-testing.md`, `docs/windows-trace-json-export.md`,
`docs/project-status.md`, this file, and the result file.

## Final report (42 points)

Branch; starting branch; version old/new; schema old/new; files changed;
local offer; local answer; peer-initiated offer; re-INVITE/UPDATE; hold/
resume; setup roles; incoming peer association; multiple calls; early
dialogs; forking; TCP; TLS; transport policy; fallback; recovery; UI
diagnostics; export v3; comparator; SIP-Server-RTT; Blink; AG Projects;
Linphone; PASS; FAIL; BLOCKED; NOT RUN; client problems; server problems;
automated tests; manual tests; regressions; limitations; what moves to
W103; commits; git status; push status; explicit no-merge confirmation.
