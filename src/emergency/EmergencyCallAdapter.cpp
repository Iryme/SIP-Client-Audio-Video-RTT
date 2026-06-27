#include "emergency/EmergencyCallAdapter.h"

#include <QAtomicInt>
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

SipCallOptions EmergencyCallAdapter::toLocationUpdateOptions(const QString &pidfLo,
                                                              const QString &contentId)
{
    SipCallOptions opts;
    opts.emergencyCall = true;
    opts.body          = pidfLo;
    opts.contentType   = QStringLiteral("application/pidf+xml");
    opts.contentId     = contentId;
    opts.requireAudio  = false;
    opts.requireRtt    = false;
    opts.allowVideo    = false;

    if (!contentId.isEmpty())
        opts.customHeaders.append(
            {QStringLiteral("Geolocation"),
             QStringLiteral("<cid:%1>").arg(contentId)});
    opts.customHeaders.append(
        {QStringLiteral("Geolocation-Routing"), QStringLiteral("yes")});
    opts.customHeaders.append(
        {QStringLiteral("Supported"), QStringLiteral("geolocation")});

    return opts;
}

QString EmergencyCallAdapter::generateContentId()
{
    static QAtomicInt s_counter{0};
    const qint64 ms  = QDateTime::currentMSecsSinceEpoch();
    const int    seq = s_counter.fetchAndAddRelaxed(1);
    return QStringLiteral("pidflo-%1-%2@ng112.local").arg(ms).arg(seq);
}
