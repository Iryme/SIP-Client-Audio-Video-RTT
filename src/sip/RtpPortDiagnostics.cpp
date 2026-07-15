#include "sip/RtpPortDiagnostics.h"

QString rtpPortErrorKindName(RtpPortErrorKind kind)
{
    switch (kind) {
    case RtpPortErrorKind::NotPortRelated:        return QStringLiteral("NotPortRelated");
    case RtpPortErrorKind::RtpPortInUse:          return QStringLiteral("RtpPortInUse");
    case RtpPortErrorKind::RtpPortRangeExhausted: return QStringLiteral("RtpPortRangeExhausted");
    case RtpPortErrorKind::RtpPortAllocationFailed: return QStringLiteral("RtpPortAllocationFailed");
    }
    return QStringLiteral("Unknown");
}

RtpPortErrorKind classifyRtpPortError(const QString &reason)
{
    const QString r = reason.toLower();

    const bool mentionsPortOrBind =
        r.contains(QStringLiteral("port")) || r.contains(QStringLiteral("bind"))
        || r.contains(QStringLiteral("address"));

    if (r.contains(QStringLiteral("wsaeaddrinuse"))
        || r.contains(QStringLiteral("eaddrinuse"))
        || r.contains(QStringLiteral("already in use"))) {
        return RtpPortErrorKind::RtpPortInUse;
    }

    if (mentionsPortOrBind
        && (r.contains(QStringLiteral("exhaust"))
            || r.contains(QStringLiteral("no free"))
            || r.contains(QStringLiteral("unable to find"))
            || r.contains(QStringLiteral("out of range")))) {
        return RtpPortErrorKind::RtpPortRangeExhausted;
    }

    if (mentionsPortOrBind || r.contains(QStringLiteral("transport"))) {
        return RtpPortErrorKind::RtpPortAllocationFailed;
    }

    return RtpPortErrorKind::NotPortRelated;
}
