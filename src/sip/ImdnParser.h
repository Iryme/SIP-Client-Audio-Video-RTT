#pragma once
#include "sip/ImdnInfo.h"

// Parses a message/imdn+xml body into an ImdnInfo. Pure Qt/text-based
// (QXmlStreamReader) — no PJSIP types — so it can be unit tested without a
// live PJSIP stack.
class ImdnParser
{
public:
    static ImdnInfo parse(const QString &body);
};
