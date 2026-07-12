# Client/Server Trace Comparison (Task W102 Phase 11)

Covers `TraceComparator::compareTraces` (`src/sip/TraceComparator.{h,cpp}`),
tested in `tests/test_trace_comparator.cpp`.

## What it does

A pure function — no file I/O, no network access, deterministic — that
takes two already-exported `InteropTraceExporter` JSON documents (see
[windows-trace-json-export.md](windows-trace-json-export.md)) and
correlates their `"events"` arrays by `(callId, cseq)`, the one identifier
pair both sides of a SIP dialog leg are guaranteed to share verbatim.

For each correlated pair or unmatched entry it reports one of:

| kind | meaning |
|---|---|
| `missingOnRight` | present in the left trace, no counterpart in the right |
| `missingOnLeft` | present in the right trace, no counterpart in the left |
| `duplicate` | the same `(callId, cseq)` key appears more than once on one side |
| `directionMismatch` | the right side's direction isn't the mirror (outbound↔inbound) of the left side's |
| `payloadTypeMismatch` | both sides have a payloadType but they disagree |
| `timestampOutOfTolerance` | correlated events' timestamps differ by more than `timestampToleranceMs` (default 5000ms, configurable — clock skew between two machines is expected and not itself an error) |

Output is a `QJsonObject` with a `summary` (event counts, correlated count,
mismatch count) and a `mismatches` array — machine-readable, suitable for
CI gating or a future UI panel.

## Why it only compares two client-schema exports right now

Task W094 (a prior task, referenced in
[windows-trace-json-export.md](windows-trace-json-export.md)) already
documented that this repository does not contain
`SIP-Server-RTT`'s actual `scripts/interop/compare-client-server-trace.py`
output format — it was not present at the time W094 was written, and it
still is not present in this repository as of W102. `TraceComparator`
therefore assumes both inputs already conform to this client's own
`InteropTraceExporter` schema (`callId`/`cseq`/`direction`/`payloadType`/
`timestamp` fields per event).

**This is not a limitation invented for convenience** — Task rule 5 ("Nu
inventa formate protocolare") forbids guessing at a server export schema
that hasn't been confirmed. If/when a real `SIP-Server-RTT` export sample
becomes available, a small adapter converting it into the same field names
consumed here (or extending `compareTraces` to accept a schema hint) is the
correct next step — not a fabricated assumption now.

## Test coverage

`tests/test_trace_comparator.cpp` (8 tests, all passing): identical traces
produce zero mismatches; missing-on-right; missing-on-left; duplicate;
direction mismatch; payload-type mismatch; timestamp-out-of-tolerance;
determinism across repeated calls with identical input.

## What was NOT run

No comparison against a real second export from SIP-Server-RTT, Blink, AG
Projects, or Linphone was performed — no such export was available in this
environment. See
[msrp-live-interoperability.md](msrp-live-interoperability.md) for the full
PASS/FAIL/BLOCKED/NOT RUN breakdown of live interoperability testing.
