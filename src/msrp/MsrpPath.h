#pragma once
#include <QString>
#include <QStringList>

#include "msrp/MsrpTypes.h"

// RFC 4975 MSRP URI: msrp-uri = ("msrp" / "msrps") "://" authority "/"
// session-id ";" transport. Pure text parsing, no network/DNS access.
struct MsrpUri
{
    bool ok{false};
    QString scheme;     // "msrp" / "msrps", lowercased
    QString host;       // hostname, IPv4, or bracketed-stripped IPv6
    bool hostIsIpv6{false};
    int port{-1};        // -1 = not specified (default 2855/2856 per RFC 4975/4976)
    QString sessionId;
    QString transport;  // usually "tcp"
    QString errorMessage;

    MsrpTransportProtocol transportProtocol() const
    {
        if (scheme == QLatin1String("msrps")) return MsrpTransportProtocol::Tls;
        if (scheme == QLatin1String("msrp"))  return MsrpTransportProtocol::Tcp;
        return MsrpTransportProtocol::Unknown;
    }

    QString toString() const
    {
        QString hostPart = hostIsIpv6 ? (QLatin1Char('[') + host + QLatin1Char(']')) : host;
        QString s = scheme + QStringLiteral("://") + hostPart;
        if (port > 0)
            s += QStringLiteral(":%1").arg(port);
        s += QLatin1Char('/') + sessionId + QLatin1Char(';') + transport;
        return s;
    }
};

namespace MsrpPath {

MsrpUri parseUri(const QString &text);

// A path is a space-separated list of MSRP URIs (RFC 4975 a=path).
QList<MsrpUri> parsePath(const QString &text);
QString buildPath(const QList<MsrpUri> &uris);

// Cryptographically-random, URL-safe session id (RFC 4975 recommends >=
// 80 bits of randomness). Never derived from user/host identifiers.
QString generateSessionId();

// Builds a single msrp(s):// URI from components (host/port from
// runtime/config, never hardcoded by the caller of this function).
MsrpUri buildUri(bool useTls, const QString &host, int port, const QString &sessionId);

} // namespace MsrpPath
