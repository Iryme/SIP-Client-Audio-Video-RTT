#pragma once
#include <QList>
#include <QPair>
#include <QString>

// Per-call SIP options passed to SipCall::makeCallWithOptions() and
// SipManager::makeEmergencyCall().  A default-constructed instance
// (isEmpty() == true) produces behavior identical to makeCall().
struct SipCallOptions
{
    bool    emergencyCall    = false;
    QString requestUriOverride;                   // reserved — not yet wired
    QList<QPair<QString,QString>> customHeaders;  // injected into INVITE txOption
    QString body;                                 // PIDF-LO body for multipart/mixed INVITE
    QString contentType;
    QString contentId;
    // Media defaults: audio-only. Video and RTT are offered only when the
    // caller explicitly selects them (call-type combo / Request buttons) or
    // requests them mid-call via re-INVITE.
    bool    requireAudio = true;    // offer m=audio
    bool    requireRtt   = false;   // offer m=text (RFC 4103)
    bool    allowVideo   = false;   // offer m=video

    bool isEmpty() const
    {
        return !emergencyCall
            && requestUriOverride.isEmpty()
            && customHeaders.isEmpty()
            && body.isEmpty();
    }

    static SipCallOptions normal() { return {}; }
};
