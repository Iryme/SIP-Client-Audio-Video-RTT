#pragma once
#include <QList>
#include <QString>

#include "sip/MessagingTraceEntry.h"

// Exports Messaging/MSRP diagnostics (SIP MESSAGE, CPIM, IMDN, is-composing,
// SDP MSRP attributes) as JSON intended to be comparable with the
// SIP-Server-RTT project's scripts/interop/compare-client-server-trace.py
// output (Task W094).
//
// This repository does not contain compare-client-server-trace.py — it was
// not present at the time this was written. The schema below was built
// directly from Task W094's field specification; every field-naming choice
// made where the spec was ambiguous (e.g. camelCase vs. literal SDP
// attribute names, what "status" means) is documented in
// docs/windows-trace-json-export.md, along with a worked sample export.
//
// Strictly diagnostic: never opens an MSRP socket, never starts a real MSRP
// session, and never modifies call/media behavior. No new parsing is
// performed here — every field is read from MessagingTraceEntry structures
// already built by MessagingDiagnosticsStore (Task W090), and
// transport/payloadType/status classification is reused as-is from
// MessagingEventStore::mapFromTraceEntry (Task W091).
namespace InteropTraceExporter {

// Bumped 1 -> 2 for Task W095: adds contentEncoding/decodeStatus/
// decodeVariant/compressedBodyLength/decodedBodyLength/decodeError and (when
// applicable) a nested "rcsFileTransfer" object to every event. All v1
// fields are unchanged and still present — a v1-only consumer that ignores
// unrecognized fields continues to work unmodified; only a consumer that
// wants the new decode/RCS diagnostics needs to be aware of v2.
constexpr int kSchemaVersion = 2;

// Pure function: builds the full JSON document text from an explicit list
// of trace entries (event ids assigned 1-based by position). Fully
// unit-testable without a live MessagingDiagnosticsStore/SipTraceLogger
// signal chain.
QString exportToJson(const QList<MessagingTraceEntry> &entries);

// Convenience overload: exports the live MessagingDiagnosticsStore's
// current entries.
QString exportToJson();

} // namespace InteropTraceExporter
