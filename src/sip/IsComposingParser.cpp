#include "IsComposingParser.h"

#include <QXmlStreamReader>

namespace {

bool nameIs(const QStringView &name, const char *literal)
{
    return name.compare(QLatin1String(literal), Qt::CaseInsensitive) == 0;
}

} // namespace

IsComposingInfo IsComposingParser::parse(const QString &body)
{
    IsComposingInfo info;
    if (body.trimmed().isEmpty())
        return info;

    QXmlStreamReader xml(body);
    bool sawRoot = false;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement)
            continue;

        const QStringView name = xml.name();

        if (nameIs(name, "isComposing") || nameIs(name, "iscomposing"))
            sawRoot = true;

        if (nameIs(name, "state")) {
            const QString value = xml.readElementText().trimmed().toLower();
            if (value == QLatin1String("active"))
                info.state = IsComposingInfo::State::Active;
            else if (value == QLatin1String("idle"))
                info.state = IsComposingInfo::State::Idle;
            else if (value == QLatin1String("gone"))
                info.state = IsComposingInfo::State::Gone;
        } else if (nameIs(name, "refresh")) {
            info.refresh = xml.readElementText();
        } else if (nameIs(name, "timeout")) {
            info.timeout = xml.readElementText();
        } else if (nameIs(name, "contenttype")) {
            info.contentType = xml.readElementText();
        }
    }

    info.present = sawRoot || info.state != IsComposingInfo::State::Unknown;
    return info;
}
