#pragma once
#include "sip/SipMessageTrace.h"

// Parses a full raw SIP message (request-line/status-line + headers + body)
// into a SipMessageTrace. Pure Qt/text-based — no PJSIP types — so it can be
// unit tested without a live PJSIP stack and reused for both the real PJSIP
// capture path and any synthetic/stub trace construction.
class SipRawMessageParser
{
public:
    static SipMessageTrace parse(const QString &rawText, SipMessageTrace::Direction direction);
};
