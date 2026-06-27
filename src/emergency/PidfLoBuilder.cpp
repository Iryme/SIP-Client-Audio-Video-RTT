#include "emergency/PidfLoBuilder.h"

PidfLoBuilder::PidfLoBuilder(const EmergencyLocation &location)
    : m_location(location)
{}

PidfLoBuilder &PidfLoBuilder::setEntity(const QString &entity)
{
    m_entity = entity;
    return *this;
}

PidfLoBuilder &PidfLoBuilder::setTupleId(const QString &id)
{
    m_tupleId = id;
    return *this;
}

PidfLoBuilder &PidfLoBuilder::setContentId(const QString &cid)
{
    m_contentId = cid;
    return *this;
}

// static
QString PidfLoBuilder::contentType()
{
    return QStringLiteral("application/pidf+xml");
}

// static
QString PidfLoBuilder::escapeXml(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '&':  out += QStringLiteral("&amp;");  break;
        case '<':  out += QStringLiteral("&lt;");   break;
        case '>':  out += QStringLiteral("&gt;");   break;
        default:   out += c;                        break;
        }
    }
    return out;
}

// static
QString PidfLoBuilder::escapeXmlAttr(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '&':  out += QStringLiteral("&amp;");  break;
        case '<':  out += QStringLiteral("&lt;");   break;
        case '>':  out += QStringLiteral("&gt;");   break;
        case '"':  out += QStringLiteral("&quot;"); break;
        default:   out += c;                        break;
        }
    }
    return out;
}

PidfLoResult PidfLoBuilder::build() const
{
    PidfLoResult result;

    const QStringList errors = m_location.validationErrors();
    if (!errors.isEmpty()) {
        result.success = false;
        result.error   = errors.join(QStringLiteral("; "));
        return result;
    }

    const QString entity    = m_entity.isEmpty()
                              ? QStringLiteral("pres:anonymous@ng112.local")
                              : m_entity;
    const QString tupleId   = m_tupleId.isEmpty()
                              ? QStringLiteral("loc001")
                              : m_tupleId;
    const QString contentId = m_contentId.isEmpty()
                              ? QStringLiteral("pidflo-1@ng112.local")
                              : m_contentId;

    const QString lat = QString::number(m_location.latitude,  'f', 6);
    const QString lon = QString::number(m_location.longitude, 'f', 6);
    const QString ts  = escapeXml(m_location.timestamp);

    QString xml;
    xml.reserve(1024);

    xml += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    xml += QStringLiteral("<presence\n");
    xml += QStringLiteral("  xmlns=\"urn:ietf:params:xml:ns:pidf\"\n");
    xml += QStringLiteral("  xmlns:gp=\"urn:ietf:params:xml:ns:pidf:geopriv10\"\n");
    xml += QStringLiteral("  xmlns:gml=\"http://www.opengis.net/gml\"\n");
    xml += QStringLiteral("  xmlns:gs=\"http://www.opengis.net/pidflo/1.0\"\n");
    xml += QStringLiteral("  entity=\"") + escapeXmlAttr(entity) + QStringLiteral("\">\n");
    xml += QStringLiteral("  <tuple id=\"") + escapeXmlAttr(tupleId) + QStringLiteral("\">\n");
    xml += QStringLiteral("    <status><basic>open</basic></status>\n");
    xml += QStringLiteral("    <gp:geopriv>\n");
    xml += QStringLiteral("      <gp:location-info>\n");

    if (m_location.uncertaintyMeters >= 0.0) {
        // Encode as Circle (RFC 5491 §5) when uncertainty is available.
        const QString radius = QString::number(m_location.uncertaintyMeters, 'f', 1);
        xml += QStringLiteral("        <gs:Circle srsName=\"urn:ogc:def:crs:EPSG::4326\">\n");
        xml += QStringLiteral("          <gml:pos>") + lat + QStringLiteral(" ") + lon
               + QStringLiteral("</gml:pos>\n");
        xml += QStringLiteral("          <gs:radius uom=\"urn:ogc:def:uom:EPSG::9001\">")
               + radius + QStringLiteral("</gs:radius>\n");
        xml += QStringLiteral("        </gs:Circle>\n");
    } else {
        // Encode as Point (RFC 4119 §3).
        xml += QStringLiteral("        <gml:Point srsName=\"urn:ogc:def:crs:EPSG::4326\">\n");
        xml += QStringLiteral("          <gml:pos>") + lat + QStringLiteral(" ") + lon
               + QStringLiteral("</gml:pos>\n");
        xml += QStringLiteral("        </gml:Point>\n");
    }

    xml += QStringLiteral("      </gp:location-info>\n");
    xml += QStringLiteral("      <gp:usage-rules/>\n");
    xml += QStringLiteral("    </gp:geopriv>\n");
    xml += QStringLiteral("    <timestamp>") + ts + QStringLiteral("</timestamp>\n");
    xml += QStringLiteral("  </tuple>\n");
    xml += QStringLiteral("</presence>\n");

    result.success     = true;
    result.xml         = xml;
    result.contentType = contentType();
    result.contentId   = contentId;
    return result;
}
