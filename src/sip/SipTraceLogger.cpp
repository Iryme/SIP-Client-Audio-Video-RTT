#include "SipTraceLogger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "core/Logger.h"

SipTraceLogger &SipTraceLogger::instance()
{
    static SipTraceLogger s;
    return s;
}

SipTraceLogger::SipTraceLogger() : QObject(nullptr)
{
    qRegisterMetaType<SipMessageTrace>("SipMessageTrace");
}

void SipTraceLogger::logMessage(const SipMessageTrace &trace)
{
    SipMessageTrace sanitized = trace;

    if (sanitized.timestamp.isNull())
        sanitized.timestamp = QDateTime::currentDateTime();

    if (!sanitized.rawSip.isEmpty())
        sanitized.rawSip = redactCredentials(sanitized.rawSip);

    m_messages.append(sanitized);

    // When a raw SIP body is present, log it at Raw level (off by default).
    if (!sanitized.rawSip.isEmpty())
        Logger::instance().raw(LogCategory::Sip, sanitized.summary(), sanitized.rawSip);

    emit messageLogged(sanitized);
}

const QList<SipMessageTrace> &SipTraceLogger::messages() const
{
    return m_messages;
}

void SipTraceLogger::clear()
{
    m_messages.clear();
    emit cleared();
}

QString SipTraceLogger::exportToText() const
{
    QString out;
    QTextStream ts(&out);
    for (const auto &t : m_messages) {
        ts << t.timestamp.toString(Qt::ISODateWithMs)
           << (t.direction == SipMessageTrace::Direction::Outbound
               ? QStringLiteral(" >>> ") : QStringLiteral(" <<< "))
           << t.summary();
        if (!t.fromUri.isEmpty())
            ts << QStringLiteral("  From: ") << t.fromUri;
        if (!t.toUri.isEmpty())
            ts << QStringLiteral("  To: ") << t.toUri;
        if (!t.cSeq.isEmpty())
            ts << QStringLiteral("  CSeq: ") << t.cSeq;
        if (!t.callId.isEmpty())
            ts << QStringLiteral("  Call-ID: ") << t.callId;
        ts << '\n';
    }
    return out;
}

QString SipTraceLogger::exportToJson() const
{
    QJsonArray arr;
    for (const auto &t : m_messages) {
        QJsonObject obj;
        obj[QStringLiteral("timestamp")] = t.timestamp.toString(Qt::ISODateWithMs);
        obj[QStringLiteral("direction")] =
            t.direction == SipMessageTrace::Direction::Outbound
                ? QStringLiteral("outbound") : QStringLiteral("inbound");
        obj[QStringLiteral("method")]     = t.method;
        obj[QStringLiteral("statusCode")] = t.statusCode;
        obj[QStringLiteral("statusText")] = t.statusText;
        obj[QStringLiteral("from")]       = t.fromUri;
        obj[QStringLiteral("to")]         = t.toUri;
        obj[QStringLiteral("callId")]     = t.callId;
        obj[QStringLiteral("cseq")]       = t.cSeq;
        arr.append(obj);
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Indented);
}

QString SipTraceLogger::redactCredentials(const QString &raw)
{
    QStringList lines = raw.split('\n');
    for (auto &line : lines) {
        // Trim carriage return for comparison but preserve it in output.
        const QString trimmed = line.endsWith('\r')
            ? line.left(line.size() - 1) : line;
        if (trimmed.startsWith(QStringLiteral("Authorization:"), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Proxy-Authorization:"), Qt::CaseInsensitive)) {
            const int colon = line.indexOf(':');
            if (colon >= 0)
                line = line.left(colon + 1) + QStringLiteral(" [REDACTED]");
        }
    }
    return lines.join('\n');
}
