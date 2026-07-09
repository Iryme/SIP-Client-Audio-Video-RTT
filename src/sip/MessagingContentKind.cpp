#include "MessagingContentKind.h"

MessagingContentKind MessagingContentKindDetector::detect(const QString &contentType)
{
    const QString ct = contentType.trimmed().toLower();
    if (ct.isEmpty())
        return MessagingContentKind::Unknown;

    // Content-Type headers may carry parameters, e.g. "text/plain; charset=utf-8".
    const int semicolon = ct.indexOf(QLatin1Char(';'));
    const QString base = (semicolon >= 0 ? ct.left(semicolon) : ct).trimmed();

    if (base == QLatin1String("text/plain"))
        return MessagingContentKind::PlainText;
    if (base == QLatin1String("text/html"))
        return MessagingContentKind::Html;
    if (base == QLatin1String("message/cpim"))
        return MessagingContentKind::Cpim;
    if (base == QLatin1String("message/imdn+xml"))
        return MessagingContentKind::Imdn;
    if (base == QLatin1String("application/im-iscomposing+xml"))
        return MessagingContentKind::IsComposing;
    if (base == QLatin1String("application/sdp"))
        return MessagingContentKind::Sdp;

    return MessagingContentKind::Unknown;
}

QString MessagingContentKindDetector::toString(MessagingContentKind kind)
{
    switch (kind) {
    case MessagingContentKind::PlainText:   return QStringLiteral("text/plain");
    case MessagingContentKind::Html:        return QStringLiteral("text/html");
    case MessagingContentKind::Cpim:        return QStringLiteral("message/cpim");
    case MessagingContentKind::Imdn:        return QStringLiteral("message/imdn+xml");
    case MessagingContentKind::IsComposing: return QStringLiteral("application/im-iscomposing+xml");
    case MessagingContentKind::Sdp:         return QStringLiteral("application/sdp");
    case MessagingContentKind::Unknown:     break;
    }
    return QStringLiteral("unknown");
}
