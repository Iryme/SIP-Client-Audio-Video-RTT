#pragma once
#include <QString>

#include "emergency/EmergencyInviteBuilder.h"
#include "sip/SipCallOptions.h"

// Bridges the pure-declarative emergency module to the SIP layer.
// No PJSIP dependency — operates purely on data structs.
class EmergencyCallAdapter
{
public:
    // Convert a built EmergencyInvite to SipCallOptions ready for
    // SipManager::makeEmergencyCall() / SipCall::makeCallWithOptions().
    // All EmergencyInvite headers become customHeaders in SipCallOptions;
    // media policy fields are mapped to requireAudio/requireRtt/allowVideo.
    static SipCallOptions toSipCallOptions(const EmergencyInvite &invite);

    // Generate a unique Content-ID for a PIDF-LO MIME part.
    // Format: pidflo-<ms-since-epoch>@ng112.local
    // Two calls within the same millisecond return the same value; that is
    // acceptable for sequential INVITE construction.
    static QString generateContentId();
};
