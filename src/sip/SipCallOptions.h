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
    QString body;                                 // reserved for multipart body (Task 40)
    QString contentType;
    QString contentId;
    bool    requireAudio = true;   // offer m=audio
    bool    requireRtt   = true;   // offer m=text (RFC 4103)
    bool    allowVideo   = true;   // offer m=video

    bool isEmpty() const
    {
        return !emergencyCall
            && requestUriOverride.isEmpty()
            && customHeaders.isEmpty()
            && body.isEmpty();
    }

    static SipCallOptions normal() { return {}; }
};
