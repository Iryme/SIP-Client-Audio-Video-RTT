#include "UrlRedactor.h"

#include <QCryptographicHash>
#include <QUrl>

namespace UrlRedactor {

QString redact(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return QString();

    const QUrl parsed(trimmed, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.scheme().isEmpty() || parsed.host().isEmpty()) {
        // Not a well-formed absolute URL — still redact conservatively by
        // hashing the whole opaque string rather than echoing it verbatim.
        const QByteArray hash = QCryptographicHash::hash(trimmed.toUtf8(), QCryptographicHash::Sha256);
        return QStringLiteral("redacted:%1").arg(QString::fromLatin1(hash.left(8).toHex()));
    }

    QString out = parsed.scheme() + QStringLiteral("://") + parsed.host();
    if (parsed.port() != -1)
        out += QStringLiteral(":%1").arg(parsed.port());

    // Query parameters (and any URL fragment) may carry a one-time download
    // token — never included, even truncated.
    const QString path = parsed.path();
    const QStringList segments = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    if (!segments.isEmpty()) {
        out += QLatin1Char('/') + segments.first();
        if (segments.size() > 1) {
            // Remaining path segments (and any query/fragment) are folded
            // into a short fingerprint so the same source URL always
            // redacts to the same value, without exposing its contents.
            const QByteArray hash = QCryptographicHash::hash(
                (path + QLatin1Char('?') + parsed.query()).toUtf8(), QCryptographicHash::Sha256);
            out += QStringLiteral("/…+%1").arg(QString::fromLatin1(hash.left(8).toHex()));
        }
    }

    return out;
}

} // namespace UrlRedactor
