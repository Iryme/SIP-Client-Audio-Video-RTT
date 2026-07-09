#pragma once
#include "sip/SdpMsrpInfo.h"

// Detects MSRP-related attributes in an SDP body (m=message media block).
// Pure Qt/text-based — no PJSIP types — so it can be unit tested without a
// live PJSIP stack. Detection only: never opens a socket or starts a
// session.
class SdpMsrpDiagnosticsParser
{
public:
    static SdpMsrpInfo parse(const QString &sdpText);
};
