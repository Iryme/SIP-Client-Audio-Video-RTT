#include "ImdnParser.h"

#include <QXmlStreamReader>

namespace {

bool nameIs(const QStringView &name, const char *literal)
{
    return name.compare(QLatin1String(literal), Qt::CaseInsensitive) == 0;
}

} // namespace

ImdnInfo ImdnParser::parse(const QString &body)
{
    ImdnInfo info;
    if (body.trimmed().isEmpty())
        return info;

    QXmlStreamReader xml(body);
    bool sawImdnRoot = false;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement)
            continue;

        const QStringView name = xml.name();

        if (nameIs(name, "imdn"))
            sawImdnRoot = true;

        if (nameIs(name, "message-id")) {
            info.messageId = xml.readElementText();
        } else if (nameIs(name, "original-recipient")) {
            info.originalRecipient = xml.readElementText(
                QXmlStreamReader::SkipChildElements);
        } else if (nameIs(name, "final-recipient")) {
            info.finalRecipient = xml.readElementText(
                QXmlStreamReader::SkipChildElements);
        } else if (info.disposition == ImdnInfo::Disposition::None) {
            if (nameIs(name, "delivered"))
                info.disposition = ImdnInfo::Disposition::Delivered;
            else if (nameIs(name, "displayed"))
                info.disposition = ImdnInfo::Disposition::Displayed;
            else if (nameIs(name, "failed"))
                info.disposition = ImdnInfo::Disposition::Failed;
            else if (nameIs(name, "error"))
                info.disposition = ImdnInfo::Disposition::Error;
        }
    }

    info.present = sawImdnRoot || !info.messageId.isEmpty()
                   || info.disposition != ImdnInfo::Disposition::None;
    return info;
}
