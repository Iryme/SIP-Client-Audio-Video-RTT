#include "emergency/EmergencyMultipartBuilder.h"

EmergencyMultipartBuilder &EmergencyMultipartBuilder::addPidfLo(
    const QString &xml, const QString &contentId)
{
    if (xml.isEmpty())
        return *this;

    MultipartPart part;
    part.contentType = QStringLiteral("application/pidf+xml");
    part.contentId   = contentId;
    part.body        = xml;

    // RFC 2183: Content-ID header value must include angle brackets.
    if (!contentId.isEmpty())
        part.headers.append({QStringLiteral("Content-ID"),
                             QStringLiteral("<%1>").arg(contentId)});

    m_parts.append(part);
    return *this;
}

QList<MultipartPart> EmergencyMultipartBuilder::build() const
{
    return m_parts;
}

bool EmergencyMultipartBuilder::isEmpty() const
{
    return m_parts.isEmpty();
}
