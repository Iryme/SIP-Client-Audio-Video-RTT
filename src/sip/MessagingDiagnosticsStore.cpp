#include "MessagingDiagnosticsStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

#include "sip/CpimParser.h"
#include "sip/ImdnParser.h"
#include "sip/IsComposingParser.h"
#include "sip/SdpMsrpDiagnosticsParser.h"
#include "sip/SipTraceLogger.h"

namespace {

QString normalizeLineEndings(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString extractBody(const QString &rawSip)
{
    const QString normalized = normalizeLineEndings(rawSip);
    const int sep = normalized.indexOf(QStringLiteral("\n\n"));
    if (sep < 0)
        return QString();
    return normalized.mid(sep + 2).trimmed();
}

// Safe, compact single-line preview for the UI/export — collapses
// whitespace/newlines and truncates. This is a display convenience, not a
// security redaction (credential redaction happens upstream in
// SipTraceLogger::redactCredentials before the trace ever reaches here).
QString makeBodyPreview(const QString &body, int maxLen = 160)
{
    QString collapsed = body;
    collapsed.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    collapsed = collapsed.trimmed();
    if (collapsed.size() > maxLen)
        return collapsed.left(maxLen) + QStringLiteral("…");
    return collapsed;
}

bool looksLikeMessagingContentType(const QString &contentType)
{
    const MessagingContentKind kind = MessagingContentKindDetector::detect(contentType);
    return kind == MessagingContentKind::Cpim
        || kind == MessagingContentKind::Imdn
        || kind == MessagingContentKind::IsComposing;
}

bool bodyHasSdpMessageMedia(const QString &body)
{
    for (const QString &rawLine : body.split(QLatin1Char('\n'))) {
        if (rawLine.trimmed().startsWith(QStringLiteral("m=message")))
            return true;
    }
    return false;
}

} // namespace

MessagingDiagnosticsStore &MessagingDiagnosticsStore::instance()
{
    static MessagingDiagnosticsStore s;
    return s;
}

MessagingDiagnosticsStore::MessagingDiagnosticsStore() : QObject(nullptr)
{
    qRegisterMetaType<MessagingTraceEntry>("MessagingTraceEntry");
    connect(&SipTraceLogger::instance(), &SipTraceLogger::messageLogged,
            this, &MessagingDiagnosticsStore::onSipMessageLogged);
}

bool MessagingDiagnosticsStore::isMessagingRelevant(const SipMessageTrace &trace)
{
    if (trace.method.compare(QStringLiteral("MESSAGE"), Qt::CaseInsensitive) == 0)
        return true;

    if (looksLikeMessagingContentType(trace.contentType))
        return true;

    const QString body = extractBody(trace.rawSip);
    if (bodyHasSdpMessageMedia(body))
        return true;

    return false;
}

MessagingTraceEntry MessagingDiagnosticsStore::buildEntry(const SipMessageTrace &trace)
{
    MessagingTraceEntry entry;
    entry.timestamp   = trace.timestamp;
    entry.direction   = trace.direction;
    entry.method      = trace.method;
    entry.statusCode  = trace.statusCode;
    entry.statusText  = trace.statusText;
    entry.fromUri     = trace.fromUri;
    entry.toUri       = trace.toUri;
    entry.callId      = trace.callId;
    entry.cSeq        = trace.cSeq;
    entry.contentType = trace.contentType;
    entry.rawSip      = trace.rawSip;

    const QString body = extractBody(trace.rawSip);
    entry.bodyPreview  = makeBodyPreview(body);
    entry.contentKind  = MessagingContentKindDetector::detect(trace.contentType);

    QString semanticBody = body;
    MessagingContentKind semanticKind = entry.contentKind;

    if (entry.contentKind == MessagingContentKind::Cpim) {
        entry.cpim = CpimParser::parse(body);
        if (entry.cpim.present && !entry.cpim.contentType.isEmpty()) {
            semanticKind = MessagingContentKindDetector::detect(entry.cpim.contentType);
            semanticBody = entry.cpim.wrappedBody;
        }
    }

    if (semanticKind == MessagingContentKind::Imdn)
        entry.imdn = ImdnParser::parse(semanticBody);
    else if (semanticKind == MessagingContentKind::IsComposing)
        entry.isComposing = IsComposingParser::parse(semanticBody);

    if (bodyHasSdpMessageMedia(body))
        entry.sdpMsrp = SdpMsrpDiagnosticsParser::parse(body);

    return entry;
}

void MessagingDiagnosticsStore::onSipMessageLogged(const SipMessageTrace &trace)
{
    if (!isMessagingRelevant(trace))
        return;

    const MessagingTraceEntry entry = buildEntry(trace);
    m_entries.append(entry);
    emit entryLogged(entry);
}

const QList<MessagingTraceEntry> &MessagingDiagnosticsStore::entries() const
{
    return m_entries;
}

void MessagingDiagnosticsStore::clear()
{
    m_entries.clear();
    emit cleared();
}

QString MessagingDiagnosticsStore::exportToText() const
{
    QString out;
    QTextStream ts(&out);
    for (const auto &e : m_entries) {
        ts << e.timestamp.toString(Qt::ISODateWithMs)
           << (e.direction == SipMessageTrace::Direction::Outbound
               ? QStringLiteral(" >>> ") : QStringLiteral(" <<< "))
           << e.summary();
        if (!e.fromUri.isEmpty())
            ts << QStringLiteral("  From: ") << e.fromUri;
        if (!e.toUri.isEmpty())
            ts << QStringLiteral("  To: ") << e.toUri;
        if (!e.callId.isEmpty())
            ts << QStringLiteral("  Call-ID: ") << e.callId;
        if (!e.contentType.isEmpty())
            ts << QStringLiteral("  Content-Type: ") << e.contentType;
        ts << QStringLiteral("  Kind: ") << MessagingContentKindDetector::toString(e.contentKind);
        ts << '\n';

        if (!e.bodyPreview.isEmpty())
            ts << QStringLiteral("  Body preview: ") << e.bodyPreview << '\n';

        if (e.cpim.present) {
            ts << QStringLiteral("  CPIM From: ") << e.cpim.from
               << QStringLiteral("  To: ") << e.cpim.to
               << QStringLiteral("  DateTime: ") << e.cpim.dateTime
               << QStringLiteral("  Subject: ") << e.cpim.subject
               << QStringLiteral("  Content-Type: ") << e.cpim.contentType << '\n';
        }
        if (e.imdn.present) {
            ts << QStringLiteral("  IMDN disposition: ") << ImdnInfo::dispositionToString(e.imdn.disposition)
               << QStringLiteral("  Message-ID: ") << e.imdn.messageId
               << QStringLiteral("  original-recipient: ") << e.imdn.originalRecipient
               << QStringLiteral("  final-recipient: ") << e.imdn.finalRecipient << '\n';
        }
        if (e.isComposing.present) {
            ts << QStringLiteral("  is-composing state: ") << IsComposingInfo::stateToString(e.isComposing.state)
               << QStringLiteral("  timeout: ") << e.isComposing.timeout
               << QStringLiteral("  refresh: ") << e.isComposing.refresh << '\n';
        }
        if (e.sdpMsrp.present) {
            ts << QStringLiteral("  MSRP media: ") << e.sdpMsrp.mediaLine
               << QStringLiteral("  transport: ") << e.sdpMsrp.transportProtocol
               << QStringLiteral("  path: ") << e.sdpMsrp.path
               << QStringLiteral("  accept-types: ") << e.sdpMsrp.acceptTypes
               << QStringLiteral("  setup: ") << e.sdpMsrp.setup
               << QStringLiteral("  connection: ") << e.sdpMsrp.connection
               << QStringLiteral("  session-id: ") << e.sdpMsrp.sessionId << '\n';
        }

        if (!e.rawSip.isEmpty()) {
            // rawSip is already redacted (Authorization/Proxy-Authorization
            // stripped) by SipTraceLogger before this entry was built.
            ts << e.rawSip;
            if (!e.rawSip.endsWith(QLatin1Char('\n')))
                ts << '\n';
        }
        ts << QStringLiteral("--- end message ---\n");
    }
    return out;
}

QString MessagingDiagnosticsStore::exportToJson() const
{
    QJsonArray arr;
    for (const auto &e : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("timestamp")] = e.timestamp.toString(Qt::ISODateWithMs);
        obj[QStringLiteral("direction")] =
            e.direction == SipMessageTrace::Direction::Outbound
                ? QStringLiteral("outbound") : QStringLiteral("inbound");
        obj[QStringLiteral("method")]      = e.method;
        obj[QStringLiteral("statusCode")]  = e.statusCode;
        obj[QStringLiteral("statusText")]  = e.statusText;
        obj[QStringLiteral("from")]        = e.fromUri;
        obj[QStringLiteral("to")]          = e.toUri;
        obj[QStringLiteral("callId")]      = e.callId;
        obj[QStringLiteral("cseq")]        = e.cSeq;
        obj[QStringLiteral("contentType")] = e.contentType;
        obj[QStringLiteral("contentKind")] = MessagingContentKindDetector::toString(e.contentKind);
        obj[QStringLiteral("bodyPreview")] = e.bodyPreview;

        if (e.cpim.present) {
            QJsonObject cpim;
            cpim[QStringLiteral("from")]        = e.cpim.from;
            cpim[QStringLiteral("to")]          = e.cpim.to;
            cpim[QStringLiteral("dateTime")]    = e.cpim.dateTime;
            cpim[QStringLiteral("subject")]     = e.cpim.subject;
            cpim[QStringLiteral("contentType")] = e.cpim.contentType;
            obj[QStringLiteral("cpim")] = cpim;
        }
        if (e.imdn.present) {
            QJsonObject imdn;
            imdn[QStringLiteral("disposition")]       = ImdnInfo::dispositionToString(e.imdn.disposition);
            imdn[QStringLiteral("messageId")]         = e.imdn.messageId;
            imdn[QStringLiteral("originalRecipient")] = e.imdn.originalRecipient;
            imdn[QStringLiteral("finalRecipient")]    = e.imdn.finalRecipient;
            obj[QStringLiteral("imdn")] = imdn;
        }
        if (e.isComposing.present) {
            QJsonObject ic;
            ic[QStringLiteral("state")]   = IsComposingInfo::stateToString(e.isComposing.state);
            ic[QStringLiteral("timeout")] = e.isComposing.timeout;
            ic[QStringLiteral("refresh")] = e.isComposing.refresh;
            obj[QStringLiteral("isComposing")] = ic;
        }
        if (e.sdpMsrp.present) {
            QJsonObject msrp;
            msrp[QStringLiteral("mediaLine")]         = e.sdpMsrp.mediaLine;
            msrp[QStringLiteral("transportProtocol")] = e.sdpMsrp.transportProtocol;
            msrp[QStringLiteral("path")]              = e.sdpMsrp.path;
            msrp[QStringLiteral("acceptTypes")]       = e.sdpMsrp.acceptTypes;
            msrp[QStringLiteral("setup")]             = e.sdpMsrp.setup;
            msrp[QStringLiteral("connection")]        = e.sdpMsrp.connection;
            msrp[QStringLiteral("sessionId")]         = e.sdpMsrp.sessionId;
            obj[QStringLiteral("msrp")] = msrp;
        }

        // rawSip is already redacted (Authorization/Proxy-Authorization
        // values stripped) by SipTraceLogger before this entry was built.
        obj[QStringLiteral("rawSip")] = e.rawSip;

        arr.append(obj);
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Indented);
}
