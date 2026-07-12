#pragma once
#include <QString>

// MSRP <-> live SIP call integration (Task W100, section N).
//
// Architectural decision (see docs/msrp-foundation.md for the full audit):
// this vendored pjproject build has no MSRP support at any layer, and
// pjsua2's Call::onCallSdpCreated callback is read-only — OnCallSdpCreatedParam::
// sdp.wholeSdp has no toPj() to feed edits back into the actual outbound
// SDP, and mutating the raw pjSdpSession pool pointer would be exactly the
// kind of fragile manual SIP transaction this project's conventions forbid
// (see Task W098's Publish decision, Task W099's Digest decision). So this
// task does NOT inject m=message into live outbound INVITE/re-INVITE SDP.
//
// What IS implemented: detection. SipCall's existing, unmodified
// onCallSdpCreated/onCallRxReinvite observation hooks already extract the
// created/received SDP as plain text for logging — this function is called
// from that same already-existing extraction point (one additional,
// read-only call; it cannot alter call behavior since it never touches
// prm.sdp) to notice when a peer or this account's own generated SDP
// contains "m=message" and record a "Detected" MsrpSessionInfo entry in
// MsrpSessionStore. This makes real call SDP MSRP presence visible in the
// MSRP page/diagnostics without risking audio/video/RTT regressions.
//
// A real MSRP session (SEND/RECEIVE) can currently only be established via
// the independent MSRP page's manual configuration (MsrpSession, driven by
// user/config-supplied host/port/path), never automatically from a live
// call's negotiated SDP — see docs/msrp-foundation.md §"What remains for
// W101" for what full live-call wiring would require.
namespace MsrpSipIntegration {

// sdpText: the full SDP body (offer, answer, or re-offer) as already
// extracted by SipCall's existing hooks. sipCallId identifies the call for
// MsrpSessionStore's sessionKey. Never blocks, never touches PJSIP types.
void detectFromSdp(const QString &sdpText, const QString &sipCallId);

} // namespace MsrpSipIntegration
