#pragma once
#include "sip/CpimInfo.h"

// Parses a message/cpim body into a CpimInfo. Pure Qt/text-based — no PJSIP
// types — so it can be unit tested without a live PJSIP stack.
class CpimParser
{
public:
    static CpimInfo parse(const QString &body);
};
