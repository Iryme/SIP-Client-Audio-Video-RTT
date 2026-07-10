#include "RcsFtHttpParser.h"

#include <QXmlStreamReader>

namespace {

bool nameIs(const QStringView &name, const char *literal)
{
    return name.compare(QLatin1String(literal), Qt::CaseInsensitive) == 0;
}

// Consumes tokens until the matching </file-info>, filling `target` with
// whichever recognized child elements are present (or just skipping them,
// when target is nullptr — used for a <file-info type="thumbnail"> entry,
// whose fields this diagnostic view does not otherwise surface).
void parseFileInfoBody(QXmlStreamReader &xml, RcsFtHttpInfo *target)
{
    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::EndElement && nameIs(xml.name(), "file-info"))
            return;
        if (token != QXmlStreamReader::StartElement || !target)
            continue;

        const QStringView name = xml.name();
        if (nameIs(name, "file-size")) {
            bool ok = false;
            const qint64 value = xml.readElementText(QXmlStreamReader::SkipChildElements).toLongLong(&ok);
            if (ok)
                target->fileSize = value;
        } else if (nameIs(name, "file-name")) {
            target->fileName = xml.readElementText(QXmlStreamReader::SkipChildElements);
        } else if (nameIs(name, "content-type")) {
            target->contentType = xml.readElementText(QXmlStreamReader::SkipChildElements);
        } else if (nameIs(name, "data")) {
            const auto attrs = xml.attributes();
            target->dataUrl = attrs.value(QStringLiteral("url")).toString();
            target->expiresAt = attrs.value(QStringLiteral("until")).toString();
        } else if (nameIs(name, "file-disposition") || nameIs(name, "disposition")) {
            target->disposition = xml.readElementText(QXmlStreamReader::SkipChildElements);
        } else if (nameIs(name, "playing-length")) {
            target->playingLength = xml.readElementText(QXmlStreamReader::SkipChildElements);
        }
    }
}

} // namespace

RcsFtHttpInfo RcsFtHttpParser::parse(const QString &body)
{
    RcsFtHttpInfo info;
    if (body.trimmed().isEmpty())
        return info;

    QXmlStreamReader xml(body);
    bool sawFileRoot = false;
    bool sawPrimaryFileInfo = false;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement)
            continue;

        const QStringView name = xml.name();
        if (nameIs(name, "file"))
            sawFileRoot = true;

        if (nameIs(name, "file-info")) {
            const QString type = xml.attributes().value(QStringLiteral("type")).toString();
            if (type.compare(QStringLiteral("thumbnail"), Qt::CaseInsensitive) == 0) {
                info.thumbnailPresent = true;
                parseFileInfoBody(xml, nullptr);
                continue;
            }
            if (!sawPrimaryFileInfo) {
                info.fileInfoType = type.isEmpty() ? QStringLiteral("file") : type;
                parseFileInfoBody(xml, &info);
                sawPrimaryFileInfo = true;
            }
        }
    }

    info.present = sawFileRoot || sawPrimaryFileInfo;
    return info;
}
