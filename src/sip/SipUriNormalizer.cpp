#include "SipUriNormalizer.h"

namespace SipUriNormalizer {

Result normalize(const QString &input, const QString &fallbackDomain)
{
    const QString s = input.trimmed();

    if (s.isEmpty())
        return {{}, false, QStringLiteral("SIP URI must not be empty")};

    for (const QChar &ch : s) {
        if (ch.isSpace())
            return {{}, false, QStringLiteral("SIP URI must not contain spaces")};
    }

    // Already a SIP URI — pass through without modification.
    if (s.startsWith(QStringLiteral("sip:"),  Qt::CaseInsensitive) ||
        s.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive)) {
        return {s, true, {}};
    }

    if (s.contains(QLatin1Char('@'))) {
        const int atPos = s.indexOf(QLatin1Char('@'));
        if (atPos == 0)
            return {{}, false, QStringLiteral("SIP URI has no user part before '@'")};
        if (atPos == s.length() - 1)
            return {{}, false, QStringLiteral("SIP URI has no host part after '@'")};
        return {QStringLiteral("sip:") + s, true, {}};
    }

    // Extension-only input — qualify with fallback domain if available.
    if (!fallbackDomain.isEmpty())
        return {QStringLiteral("sip:") + s + QLatin1Char('@') + fallbackDomain, true, {}};

    return {{}, false, QStringLiteral("No domain — enter user@domain or sip:user@domain")};
}

} // namespace SipUriNormalizer
