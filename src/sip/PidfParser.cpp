#include "PidfParser.h"

#include <QXmlStreamReader>

namespace {

bool nameIs(const QStringView &name, const char *literal)
{
    return name.compare(QLatin1String(literal), Qt::CaseInsensitive) == 0;
}

// Recognized RPID/CIPID-style extended-activity element local names (Task
// W098 requirement 3). Matching is by local name only, regardless of
// namespace prefix or nesting — this client does not attempt full RPID/CIPID
// schema support, only recognition of these common activity tokens.
PresenceInfo::ExtendedStatus activityFromElementName(const QStringView &name)
{
    if (nameIs(name, "away"))
        return PresenceInfo::ExtendedStatus::Away;
    if (nameIs(name, "busy"))
        return PresenceInfo::ExtendedStatus::Busy;
    if (nameIs(name, "on-the-phone") || nameIs(name, "on_the_phone") || nameIs(name, "onthephone"))
        return PresenceInfo::ExtendedStatus::OnThePhone;
    if (nameIs(name, "do-not-disturb") || nameIs(name, "dnd"))
        return PresenceInfo::ExtendedStatus::DoNotDisturb;
    if (nameIs(name, "offline"))
        return PresenceInfo::ExtendedStatus::Offline;
    return PresenceInfo::ExtendedStatus::Unknown;
}

} // namespace

PresenceInfo PidfParser::parse(const QString &body)
{
    PresenceInfo info;
    info.contentType = QStringLiteral("application/pidf+xml");

    if (body.trimmed().isEmpty()) {
        info.parseStatus = PresenceInfo::ParseStatus::Error;
        info.parseWarnings << QStringLiteral("Empty PIDF body.");
        return info;
    }

    if (body.toUtf8().size() > kMaxPidfBytes) {
        info.parseStatus = PresenceInfo::ParseStatus::Error;
        info.parseWarnings << QStringLiteral(
            "PIDF body exceeds the %1-byte limit; not parsed.").arg(kMaxPidfBytes);
        return info;
    }

    QXmlStreamReader xml(body);
    // QXmlStreamReader does not resolve external entities or DTDs and never
    // performs network access, so no explicit entity-expansion hardening is
    // required beyond the size cap above.

    bool sawPresenceRoot = false;
    bool sawTuple = false;
    bool inFirstTuple = false;
    bool tupleAlreadyParsed = false; // only the first <tuple> is taken as canonical
    bool sawStatus = false;
    bool sawBasic = false;
    QString firstTupleId;
    QStringList notes;
    int tupleDepth = 0;

    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const QStringView name = xml.name();

            if (nameIs(name, "presence")) {
                sawPresenceRoot = true;
                const QString entity = xml.attributes().value(QStringLiteral("entity")).toString();
                if (!entity.isEmpty())
                    info.entityUri = entity;
                continue;
            }

            if (nameIs(name, "tuple")) {
                sawTuple = true;
                if (!tupleAlreadyParsed) {
                    inFirstTuple = true;
                    tupleDepth = 1;
                    firstTupleId = xml.attributes().value(QStringLiteral("id")).toString();
                } else if (inFirstTuple) {
                    ++tupleDepth; // nested <tuple>, defensive — PIDF doesn't nest these
                }
                continue;
            }

            if (nameIs(name, "status")) {
                if (inFirstTuple)
                    sawStatus = true;
                continue;
            }

            if (nameIs(name, "basic")) {
                if (inFirstTuple) {
                    sawBasic = true;
                    const QString value = xml.readElementText().trimmed();
                    info.basicStatus = PresenceInfo::basicStatusFromString(value);
                }
                continue;
            }

            if (nameIs(name, "contact")) {
                if (inFirstTuple) {
                    const QString priorityAttr =
                        xml.attributes().value(QStringLiteral("priority")).toString();
                    if (!priorityAttr.isEmpty())
                        info.priority = priorityAttr;
                    info.contactUri = xml.readElementText().trimmed();
                }
                continue;
            }

            if (nameIs(name, "note")) {
                const QString text = xml.readElementText().trimmed();
                if (!text.isEmpty())
                    notes << text;
                continue;
            }

            if (nameIs(name, "timestamp")) {
                if (inFirstTuple) {
                    const QString text = xml.readElementText().trimmed();
                    QDateTime ts = QDateTime::fromString(text, Qt::ISODate);
                    if (ts.isValid())
                        info.timestamp = ts;
                    else
                        info.parseWarnings << QStringLiteral("Unparseable <timestamp> value: %1").arg(text);
                }
                continue;
            }

            // Extension activity elements (RPID/CIPID-style), recognized by
            // local element name regardless of namespace/nesting.
            const PresenceInfo::ExtendedStatus activity = activityFromElementName(name);
            if (activity != PresenceInfo::ExtendedStatus::Unknown) {
                info.extendedStatus = activity;
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QStringView name = xml.name();
            if (nameIs(name, "tuple") && inFirstTuple) {
                --tupleDepth;
                if (tupleDepth <= 0) {
                    inFirstTuple = false;
                    tupleAlreadyParsed = true;
                    info.tupleId = firstTupleId;
                }
            }
        }
    }

    if (xml.hasError()) {
        info.parseStatus = PresenceInfo::ParseStatus::Error;
        info.parseWarnings << QStringLiteral("XML parse error: %1").arg(xml.errorString());
        return info;
    }

    info.note = notes.join(QStringLiteral(" / "));

    if (!sawPresenceRoot) {
        info.parseStatus = PresenceInfo::ParseStatus::Error;
        info.parseWarnings << QStringLiteral("No <presence> root element found.");
        return info;
    }

    if (info.entityUri.isEmpty()) {
        info.parseStatus = PresenceInfo::ParseStatus::Partial;
        info.parseWarnings << QStringLiteral("<presence> element has no entity attribute.");
    }

    if (!sawTuple || !sawStatus || !sawBasic) {
        info.parseStatus = PresenceInfo::ParseStatus::Partial;
        info.parseWarnings << QStringLiteral("No <tuple>/<status>/<basic> status found; PIDF is incomplete.");
    }

    // Fill in an extended status implied by the basic status when no
    // extension activity was recognized (open -> available, closed ->
    // offline), so the UI always has something to show.
    if (info.extendedStatus == PresenceInfo::ExtendedStatus::Unknown) {
        if (info.basicStatus == PresenceInfo::BasicStatus::Open)
            info.extendedStatus = PresenceInfo::ExtendedStatus::Available;
        else if (info.basicStatus == PresenceInfo::BasicStatus::Closed)
            info.extendedStatus = PresenceInfo::ExtendedStatus::Offline;
    }

    return info;
}
