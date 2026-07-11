#include "XcapUrlRedactor.h"

#include <QCryptographicHash>
#include <QStringList>
#include <QUrl>

namespace XcapUrlRedactor {

QString redact(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return QString();

    const QUrl parsed(trimmed, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.scheme().isEmpty() || parsed.host().isEmpty()) {
        const QByteArray hash = QCryptographicHash::hash(trimmed.toUtf8(), QCryptographicHash::Sha256);
        return QStringLiteral("redacted:%1").arg(QString::fromLatin1(hash.left(8).toHex()));
    }

    // Scheme/host/port are kept verbatim; userinfo (QUrl::userInfo()) is
    // never included even if the input URL carried one.
    QString out = parsed.scheme() + QStringLiteral("://") + parsed.host();
    if (parsed.port() != -1)
        out += QStringLiteral(":%1").arg(parsed.port());

    const QStringList segments = parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);

    // Keep up to 3 path segments verbatim: AUID, "users"/"global", and one
    // more (xui or document name) — enough to be useful for diagnostics
    // without exposing the full selector.
    constexpr int kKeepSegments = 3;
    const int keep = qMin(segments.size(), kKeepSegments);
    for (int i = 0; i < keep; ++i)
        out += QLatin1Char('/') + segments.at(i);

    // Any remaining path segments, plus the entire query string (which may
    // carry a sensitive node selector or auth token), are folded into a
    // short fingerprint rather than exposed.
    if (segments.size() > keep || !parsed.query().isEmpty()) {
        QString remainder;
        for (int i = keep; i < segments.size(); ++i)
            remainder += QLatin1Char('/') + segments.at(i);
        remainder += QLatin1Char('?') + parsed.query();

        const QByteArray hash = QCryptographicHash::hash(remainder.toUtf8(), QCryptographicHash::Sha256);
        out += QStringLiteral("/…+%1").arg(QString::fromLatin1(hash.left(8).toHex()));
    }

    return out;
}

} // namespace XcapUrlRedactor
