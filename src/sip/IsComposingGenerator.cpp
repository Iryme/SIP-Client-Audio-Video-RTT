#include "IsComposingGenerator.h"

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

} // namespace

QString IsComposingGenerator::generate(IsComposingInfo::State state,
                                       int refreshSeconds,
                                       const QString &contentType)
{
    if (state == IsComposingInfo::State::Unknown)
        return QString();

    QString xml;
    xml += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    xml += QStringLiteral("<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\">\n");
    xml += QStringLiteral("  <state>%1</state>\n").arg(IsComposingInfo::stateToString(state));
    if (refreshSeconds > 0)
        xml += QStringLiteral("  <refresh>%1</refresh>\n").arg(refreshSeconds);
    if (!contentType.trimmed().isEmpty())
        xml += QStringLiteral("  <contenttype>%1</contenttype>\n").arg(xmlEscape(contentType));
    xml += QStringLiteral("</isComposing>\n");
    return xml;
}
