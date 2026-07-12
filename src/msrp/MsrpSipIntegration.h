#pragma once
#include <QString>

// MSRP <-> live SIP call integration (Task W100, section N).
//
// Task W101 (docs/msrp-live-sdp-integration.md) superseded the original
// "onCallSdpCreated is read-only" conclusion this comment used to state:
// mutating OnCallSdpCreatedParam::sdp.pjSdpSession in place (never
// wholeSdp) IS a safe, public-API way to inject a real m=message section
// into the live outbound SDP — see SipCall.cpp's onCallSdpCreated (offerer
// path) and MsrpSipMediaInjector. That path now owns a real, call-scoped
// MsrpSession with its own listener.
//
// This function remains what it always was: detection. It notices when a
// peer's own SDP (or, still, our own) contains "m=message" and records a
// "Detected" MsrpSessionInfo entry — used today for the answer path, where
// live injection is not yet implemented (see the doc above for why).
namespace MsrpSipIntegration {

// sdpText: the full SDP body (offer, answer, or re-offer) as already
// extracted by SipCall's existing hooks. sipCallId identifies the call for
// MsrpSessionStore's sessionKey. Never blocks, never touches PJSIP types.
void detectFromSdp(const QString &sdpText, const QString &sipCallId);

} // namespace MsrpSipIntegration
