#include "emergency/EmergencyCallAdapter.h"

#include <QDateTime>

SipCallOptions EmergencyCallAdapter::toSipCallOptions(const EmergencyInvite &invite)
{
    SipCallOptions opts;
    opts.emergencyCall = true;
    opts.requireAudio  = invite.mediaPolicy.requireAudio;
    opts.requireRtt    = invite.mediaPolicy.requireRtt;
    opts.allowVideo    = invite.mediaPolicy.allowVideo;
    opts.body          = invite.body;
    opts.contentType   = invite.contentType;
    opts.contentId     = invite.contentId;

    for (const EmergencySipHeader &h : invite.headers)
        opts.customHeaders.append({h.name, h.value});

    return opts;
}

QString EmergencyCallAdapter::generateContentId()
{
    return QStringLiteral("pidflo-%1@ng112.local")
        .arg(QDateTime::currentMSecsSinceEpoch());
}
