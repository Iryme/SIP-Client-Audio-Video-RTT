#pragma once
#include "sip/IsComposingInfo.h"

// Parses an application/im-iscomposing+xml body into an IsComposingInfo.
// Pure Qt/text-based (QXmlStreamReader) — no PJSIP types — so it can be unit
// tested without a live PJSIP stack.
class IsComposingParser
{
public:
    static IsComposingInfo parse(const QString &body);
};
