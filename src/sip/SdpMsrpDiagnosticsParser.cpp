#include "SdpMsrpDiagnosticsParser.h"

#include <QRegularExpression>
#include <QStringList>

SdpMsrpInfo SdpMsrpDiagnosticsParser::parse(const QString &sdpText)
{
    SdpMsrpInfo info;
    if (sdpText.trimmed().isEmpty())
        return info;

    QString normalized = sdpText;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = normalized.split(QLatin1Char('\n'));

    int messageLineIndex = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith(QStringLiteral("m=message"))) {
            messageLineIndex = i;
            break;
        }
    }
    if (messageLineIndex < 0)
        return info;

    info.present   = true;
    info.mediaLine = lines[messageLineIndex].trimmed();

    // "m=message <port> <proto> <format...>"
    const QStringList mParts = info.mediaLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (mParts.size() >= 3)
        info.transportProtocol = mParts[2];

    // Attributes belonging to this media block run until the next "m=" line.
    for (int i = messageLineIndex + 1; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.startsWith(QStringLiteral("m=")))
            break;

        if (line.startsWith(QStringLiteral("a=path:")))
            info.path = line.mid(7).trimmed();
        else if (line.startsWith(QStringLiteral("a=accept-types:")))
            info.acceptTypes = line.mid(15).trimmed();
        else if (line.startsWith(QStringLiteral("a=setup:")))
            info.setup = line.mid(8).trimmed();
        else if (line.startsWith(QStringLiteral("a=connection:")))
            info.connection = line.mid(13).trimmed();
    }

    // Session-id is the path segment before ";tcp" in the last (closest)
    // MSRP URI, e.g. "msrp://host:port/session-id;tcp" -> "session-id".
    if (!info.path.isEmpty()) {
        const QStringList uris = info.path.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!uris.isEmpty()) {
            static const QRegularExpression sessionRe(QStringLiteral("/([^/;\\s]+);"));
            const QRegularExpressionMatch m = sessionRe.match(uris.last());
            if (m.hasMatch())
                info.sessionId = m.captured(1);
        }
    }

    return info;
}
