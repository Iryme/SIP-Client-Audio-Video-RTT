#pragma once
#include <QList>
#include <QString>

#include "msrp/MsrpDiagnosticsEvent.h"
#include "msrp/MsrpRelayDiagnosticsEvent.h"
#include "msrp/MsrpSessionInfo.h"
#include "sip/MessagingTraceEntry.h"
#include "sip/PresenceTraceEntry.h"
#include "sip/XcapModels.h"

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
//
// Task W096 (IMDN Foundation) adds generatedImdn/receivedImdn/
// correlatedMessageId/deliveryState to every event — purely additive on top
// of v2 (no existing field removed/renamed), so kSchemaVersion stays 2.
// Task W097 (Active is-composing) similarly adds generatedIsComposing/
// receivedIsComposing/typingState/typingRefresh/typingTimeout — also
// purely additive, kSchemaVersion stays 2.
//
// Task W098 (Presence Foundation) adds a top-level "presenceEvents" array,
// built from PresenceTraceEntry rows (SUBSCRIBE/NOTIFY traces, independent
// of the "events" array above) — a new top-level key, not a change to any
// existing event's fields, so kSchemaVersion stays 2.
//
// Task W099 (XCAP Foundation) adds a top-level "xcapEvents" array, built
// from XcapResult rows (completed GET/PUT/DELETE/HEAD operations, plain
// HTTP — independent of both "events" and "presenceEvents") — again purely
// additive, kSchemaVersion stays 2.
//
// Task W100 (MSRP Foundation) adds two top-level arrays, "msrpSessions"
// (from MsrpSessionStore) and "msrpEvents" (from MsrpDiagnosticsStore), plus
// four additive fields on every existing "events" entry (selectedTransport/
// actualTransport/fallbackUsed/fallbackReason — msrpTransactionId/
// msrpMessageId/msrpResponseStatus/msrpReportStatus are populated only when
// that specific SIP-MESSAGE-fallback event was correlated with an MSRP
// attempt; absent otherwise). No existing field's meaning changes, so
// kSchemaVersion stays 2 — see docs/msrp-foundation.md for the explicit
// decision record.
//
// Task W102 (MSRP Live Interoperability) bumps 2 -> 3: every "msrpSessions"
// entry gains "role" (active-connector/passive-listener/unknown, from
// MsrpSessionInfo::role — real, set by MsrpSession::connectAsActive/
// listenAsPassive, not inferred), "remoteSetup", "negotiationState" (the
// existing offerAnswerState field, not previously exported), and a nested
// "peerAssociation" object {method, mediaIndex, sipHeaderCallId, confidence}
// built from the real SIP<->MSRP mapping fields added in W101
// (MsrpSessionInfo::mediaIndex/sipHeaderCallId) — "confidence" is "exact"
// only when mediaIndex was actually set by onCallSdpCreated, "unknown"
// otherwise; never fabricated. See docs/msrp-live-interoperability.md.
// All v2 fields are unchanged, so a v2-only consumer keeps working.
//
// Not yet added in W102 (documented gap, not silently omitted — see
// docs/msrp-live-interoperability.md "Ce ramane pentru W103"): a top-level
// "msrpTransactions" array aggregated across all sessions (MsrpTransactionStore
// is currently per-session, not globally aggregated by any store), a
// top-level "fallbackEvents" history log (SipManager recomputes the
// transport decision fresh on every send rather than persisting a log), an
// "interopValidation" section (would require a real second export from
// SIP-Server-RTT to compare against — see the comparator tool), and a
// dedicated "tlsDiagnostics" array beyond the transport label already on
// each session/frame event.
//
// Task W107 (MSRP Relay Authentication, RFC 4976) adds a top-level
// "msrpRelayEvents" array, built from MsrpRelayDiagnosticsStore rows
// (relay connect/AUTH challenge/allocation/refresh/recovery — independent
// of "msrpSessions"/"msrpEvents", which only ever describe MSRP-direct
// sessions). Purely additive — no existing field/array changes — so
// kSchemaVersion stays 3. Every field is pre-redacted at the source (see
// MsrpRelayDiagnosticsEvent); this exporter never has access to the raw
// nonce/digest/credential values in the first place.
constexpr int kSchemaVersion = 3;

// Pure function: builds the full JSON document text from an explicit list
// of trace entries (event ids assigned 1-based by position). Fully
// unit-testable without a live MessagingDiagnosticsStore/SipTraceLogger
// signal chain. presenceEntries/xcapEntries/msrpSessions/msrpEvents are
// optional — pass an empty list (the default) for callers that don't care
// about that section.
QString exportToJson(const QList<MessagingTraceEntry> &entries,
                      const QList<PresenceTraceEntry> &presenceEntries = QList<PresenceTraceEntry>(),
                      const QList<XcapResult> &xcapEntries = QList<XcapResult>(),
                      const QList<MsrpSessionInfo> &msrpSessions = QList<MsrpSessionInfo>(),
                      const QList<MsrpDiagnosticsEvent> &msrpEvents = QList<MsrpDiagnosticsEvent>(),
                      const QList<MsrpRelayDiagnosticsEvent> &msrpRelayEvents = QList<MsrpRelayDiagnosticsEvent>());

// Convenience overload: exports the live MessagingDiagnosticsStore's,
// PresenceDiagnosticsStore's, XcapDiagnosticsStore's, MsrpSessionStore's,
// and MsrpDiagnosticsStore's current entries.
QString exportToJson();

} // namespace InteropTraceExporter
