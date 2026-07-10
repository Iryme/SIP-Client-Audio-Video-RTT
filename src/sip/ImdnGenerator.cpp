#include "ImdnGenerator.h"

namespace {

QString xmlEscape(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return out;
}

// RFC 5438 §5.2: "delivered"/"failed"/"forbidden" are delivery-notification
// statuses; "displayed"/"error"/"processed" are display-notification
// statuses. Each status element is an empty element named after the status.
QString statusElementName(ImdnInfo::Disposition d)
{
    switch (d) {
    case ImdnInfo::Disposition::Delivered: return QStringLiteral("delivered");
    case ImdnInfo::Disposition::Displayed: return QStringLiteral("displayed");
    case ImdnInfo::Disposition::Failed:    return QStringLiteral("failed");
    case ImdnInfo::Disposition::Error:     return QStringLiteral("error");
    case ImdnInfo::Disposition::Forbidden: return QStringLiteral("forbidden");
    case ImdnInfo::Disposition::Processed: return QStringLiteral("processed");
    case ImdnInfo::Disposition::None:      break;
    }
    return QString();
}

bool isDisplayNotification(ImdnInfo::Disposition d)
{
    return d == ImdnInfo::Disposition::Displayed
        || d == ImdnInfo::Disposition::Error
        || d == ImdnInfo::Disposition::Processed;
}

} // namespace

QString ImdnGenerator::generate(const QString &messageId,
                                ImdnInfo::Disposition disposition,
                                const QString &originalRecipient,
                                const QString &finalRecipient,
                                const QDateTime &datetime)
{
    const QString status = statusElementName(disposition);
    if (messageId.trimmed().isEmpty() || status.isEmpty())
        return QString();

    const QDateTime when = datetime.isValid() ? datetime : QDateTime::currentDateTimeUtc();
    const QString notificationTag = isDisplayNotification(disposition)
        ? QStringLiteral("display-notification") : QStringLiteral("delivery-notification");

    QString xml;
    xml += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    xml += QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n");
    xml += QStringLiteral("  <message-id>%1</message-id>\n").arg(xmlEscape(messageId));
    xml += QStringLiteral("  <datetime>%1</datetime>\n")
               .arg(when.toUTC().toString(Qt::ISODateWithMs));
    // Element names match what ImdnParser (Task W090) already recognizes —
    // "original-recipient"/"final-recipient", not the RFC 5438 schema's
    // "-uri"-suffixed variants — so a report generated here round-trips
    // through the existing parser unchanged.
    if (!originalRecipient.trimmed().isEmpty())
        xml += QStringLiteral("  <original-recipient>%1</original-recipient>\n")
                   .arg(xmlEscape(originalRecipient));
    if (!finalRecipient.trimmed().isEmpty())
        xml += QStringLiteral("  <final-recipient>%1</final-recipient>\n")
                   .arg(xmlEscape(finalRecipient));
    xml += QStringLiteral("  <%1>\n").arg(notificationTag);
    xml += QStringLiteral("    <status>\n");
    xml += QStringLiteral("      <%1/>\n").arg(status);
    xml += QStringLiteral("    </status>\n");
    xml += QStringLiteral("  </%1>\n").arg(notificationTag);
    xml += QStringLiteral("</imdn>\n");
    return xml;
}
