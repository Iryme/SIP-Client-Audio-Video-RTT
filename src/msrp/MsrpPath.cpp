#include "MsrpPath.h"

#include <QRandomGenerator>
#include <QRegularExpression>

namespace MsrpPath {

MsrpUri parseUri(const QString &text)
{
    MsrpUri uri;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        uri.errorMessage = QStringLiteral("empty path entry");
        return uri;
    }

    const int schemeEnd = trimmed.indexOf(QStringLiteral("://"));
    if (schemeEnd <= 0) {
        uri.errorMessage = QStringLiteral("missing scheme://");
        return uri;
    }
    uri.scheme = trimmed.left(schemeEnd).toLower();
    if (uri.scheme != QLatin1String("msrp") && uri.scheme != QLatin1String("msrps")) {
        uri.errorMessage = QStringLiteral("unsupported scheme: %1").arg(uri.scheme);
        return uri;
    }

    QString rest = trimmed.mid(schemeEnd + 3);

    const int slashIdx = rest.indexOf(QLatin1Char('/'));
    if (slashIdx < 0) {
        uri.errorMessage = QStringLiteral("missing session-id/transport segment");
        return uri;
    }
    QString authority = rest.left(slashIdx);
    QString sessionAndTransport = rest.mid(slashIdx + 1);

    // authority = host [":" port], host may be "[ipv6]"
    if (authority.startsWith(QLatin1Char('['))) {
        const int closeBracket = authority.indexOf(QLatin1Char(']'));
        if (closeBracket < 0) {
            uri.errorMessage = QStringLiteral("unterminated IPv6 literal");
            return uri;
        }
        uri.host = authority.mid(1, closeBracket - 1);
        uri.hostIsIpv6 = true;
        const QString portPart = authority.mid(closeBracket + 1);
        if (portPart.startsWith(QLatin1Char(':'))) {
            bool okPort = false;
            uri.port = portPart.mid(1).toInt(&okPort);
            if (!okPort || uri.port <= 0 || uri.port > 65535) {
                uri.errorMessage = QStringLiteral("invalid port");
                return uri;
            }
        }
    } else {
        const int colonIdx = authority.lastIndexOf(QLatin1Char(':'));
        if (colonIdx >= 0) {
            uri.host = authority.left(colonIdx);
            bool okPort = false;
            uri.port = authority.mid(colonIdx + 1).toInt(&okPort);
            if (!okPort || uri.port <= 0 || uri.port > 65535) {
                uri.errorMessage = QStringLiteral("invalid port");
                return uri;
            }
        } else {
            uri.host = authority;
        }
    }

    if (uri.host.isEmpty()) {
        uri.errorMessage = QStringLiteral("empty host");
        return uri;
    }

    const int semiIdx = sessionAndTransport.indexOf(QLatin1Char(';'));
    if (semiIdx < 0) {
        uri.errorMessage = QStringLiteral("missing ;transport");
        return uri;
    }
    uri.sessionId = sessionAndTransport.left(semiIdx);
    // Only the transport token is required; any trailing ;param=value
    // extensions are ignored (not modeled at this foundation stage).
    QString transportAndParams = sessionAndTransport.mid(semiIdx + 1);
    const int nextSemi = transportAndParams.indexOf(QLatin1Char(';'));
    uri.transport = (nextSemi >= 0 ? transportAndParams.left(nextSemi) : transportAndParams).trimmed();

    if (uri.sessionId.isEmpty()) {
        uri.errorMessage = QStringLiteral("empty session-id");
        return uri;
    }
    if (uri.transport.isEmpty()) {
        uri.errorMessage = QStringLiteral("empty transport parameter");
        return uri;
    }

    uri.ok = true;
    return uri;
}

QList<MsrpUri> parsePath(const QString &text)
{
    QList<MsrpUri> uris;
    const QStringList tokens = text.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens)
        uris.append(parseUri(token));
    return uris;
}

QString buildPath(const QList<MsrpUri> &uris)
{
    QStringList parts;
    for (const MsrpUri &uri : uris)
        parts.append(uri.toString());
    return parts.join(QLatin1Char(' '));
}

QString generateSessionId()
{
    // 72 bits of randomness (12 chars from a 64-symbol URL-safe alphabet),
    // comfortably above RFC 4975's informative >=80-bit guidance is not met
    // exactly by 72 bits alone, so two 36-bit groups (12 symbols total,
    // 6 bits/symbol) are combined for 72 bits; extended below to 16 symbols
    // (96 bits) to clear the recommended threshold.
    static const QString kAlphabet = QStringLiteral(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
    QString id;
    id.reserve(16);
    for (int i = 0; i < 16; ++i)
        id.append(kAlphabet.at(QRandomGenerator::global()->bounded(kAlphabet.size())));
    return id;
}

MsrpUri buildUri(bool useTls, const QString &host, int port, const QString &sessionId)
{
    MsrpUri uri;
    uri.scheme = useTls ? QStringLiteral("msrps") : QStringLiteral("msrp");
    uri.host = host;
    uri.hostIsIpv6 = host.contains(QLatin1Char(':'));
    uri.port = port;
    uri.sessionId = sessionId;
    uri.transport = QStringLiteral("tcp");
    uri.ok = !host.isEmpty() && !sessionId.isEmpty();
    if (!uri.ok)
        uri.errorMessage = QStringLiteral("host/session-id required");
    return uri;
}

} // namespace MsrpPath
