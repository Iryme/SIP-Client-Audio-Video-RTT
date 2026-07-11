#pragma once
#include <QString>

#include "sip/PresenceInfo.h"

// Parses an application/pidf+xml body (RFC 3863) into a PresenceInfo. Pure
// Qt/text-based (QXmlStreamReader) — no PJSIP types — so it can be unit
// tested without a live PJSIP stack.
//
// Safety (Task W098 requirement 14): QXmlStreamReader never resolves
// external entities or fetches anything over the network, so no explicit
// hardening is needed for that. A byte-size cap is enforced before any
// parsing is attempted, so an oversized body is rejected cheaply instead of
// being handed to the XML parser.
class PidfParser
{
public:
    // Bodies larger than this are rejected outright (ParseStatus::Error,
    // a "body too large" warning) without ever being parsed.
    static constexpr int kMaxPidfBytes = 64 * 1024;

    static PresenceInfo parse(const QString &body);
};
