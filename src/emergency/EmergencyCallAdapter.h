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

    // Build SipCallOptions for an in-dialog location UPDATE (no new call).
    // Sets emergencyCall=true, injects Geolocation / Geolocation-Routing /
    // Supported headers and the PIDF-LO body.  Pass to
    // SipManager::sendEmergencyLocationUpdate().
    static SipCallOptions toLocationUpdateOptions(const QString &pidfLo,
                                                  const QString &contentId);

    // Generate a unique Content-ID for a PIDF-LO MIME part.
    // Format: pidflo-<ms>-<seq>@ng112.local  (atomic counter ensures uniqueness).
    static QString generateContentId();
};
