#pragma once
#include <QJsonObject>

// Task W102 Phase 11 — client/server trace comparator.
//
// Compares two already-exported InteropTraceExporter JSON documents (see
// docs/windows-trace-json-export.md) — typically this client's own export
// and a second export from the peer (e.g. SIP-Server-RTT, once that
// project's own export/log can be converted to the same schema; this
// repository does not contain SIP-Server-RTT's actual export format, so
// this comparator operates on two documents of *this* client's own schema
// — see docs/client-server-trace-comparison.md for exactly what was and
// was not validated against a real server in this task).
//
// Pure function: no file I/O, no network access, deterministic output —
// the same two inputs always produce the same QJsonObject result, so it is
// fully unit-testable and safe to run in CI.
namespace TraceComparator {

// Correlates "events" entries between `left` and `right` by (callId, cseq)
// — the one identifier pair guaranteed to be shared verbatim by both sides
// of the same SIP dialog leg — and reports, for each correlated pair or
// unmatched entry, one of: "missingOnRight", "missingOnLeft", "duplicate",
// "directionMismatch" (both sides show the same direction instead of
// mirrored outbound/inbound), "payloadTypeMismatch", "timestampOutOfTolerance".
// `timestampToleranceMs` bounds how far apart two correlated events'
// timestamps may be before being flagged (clock skew between two separate
// machines is expected and not itself an error).
QJsonObject compareTraces(const QJsonObject &left, const QJsonObject &right,
                           int timestampToleranceMs = 5000);

} // namespace TraceComparator
