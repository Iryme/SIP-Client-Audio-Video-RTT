#include "PresenceDiagnosticsStore.h"

#include <QRegularExpression>
#include <QStringList>

#include "sip/PidfParser.h"
#include "sip/SipBodyExtractor.h"
#include "sip/SipTraceLogger.h"

namespace {

QString normalizeLineEndings(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

// Small, local generic-header line lookup over the raw SIP text — mirrors
// SipRawMessageParser's private findHeaderValue() but is not shared with it
// (that helper is file-local); Event/Subscription-State/Expires are not
// among the fields SipRawMessageParser/SipMessageTrace already extract, so
// this reads them directly off the header block instead of duplicating any
// PIDF/content parsing.
QString findHeaderValue(const QStringList &headerLines, const QString &name)
{
    for (const QString &line : headerLines) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        if (line.left(colon).trimmed().compare(name, Qt::CaseInsensitive) == 0)
            return line.mid(colon + 1).trimmed();
    }
    return QString();
}

} // namespace

PresenceDiagnosticsStore &PresenceDiagnosticsStore::instance()
{
    static PresenceDiagnosticsStore s;
    return s;
}

PresenceDiagnosticsStore::PresenceDiagnosticsStore() : QObject(nullptr)
{
    qRegisterMetaType<PresenceTraceEntry>("PresenceTraceEntry");
    connect(&SipTraceLogger::instance(), &SipTraceLogger::messageLogged,
            this, &PresenceDiagnosticsStore::onSipMessageLogged);
}

bool PresenceDiagnosticsStore::isPresenceRelevant(const SipMessageTrace &trace)
{
    return trace.method.compare(QStringLiteral("SUBSCRIBE"), Qt::CaseInsensitive) == 0
        || trace.method.compare(QStringLiteral("NOTIFY"), Qt::CaseInsensitive) == 0;
}

PresenceTraceEntry PresenceDiagnosticsStore::buildEntry(const SipMessageTrace &trace)
{
    PresenceTraceEntry entry;
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
    entry.rawSip      = trace.rawSip; // already redacted upstream by SipTraceLogger

    const QString normalized = normalizeLineEndings(trace.rawSip);
    const int sep = normalized.indexOf(QStringLiteral("\n\n"));
    const QString headerBlock = sep >= 0 ? normalized.left(sep) : normalized;
    const QStringList headerLines = headerBlock.split(QLatin1Char('\n'));

    entry.eventPackage = findHeaderValue(headerLines, QStringLiteral("Event"));
    if (entry.eventPackage.isEmpty())
        entry.eventPackage = findHeaderValue(headerLines, QStringLiteral("o")); // compact form

    const QString subState = findHeaderValue(headerLines, QStringLiteral("Subscription-State"));
    if (!subState.isEmpty()) {
        const QStringList parts = subState.split(QLatin1Char(';'));
        if (!parts.isEmpty())
            entry.subscriptionState = parts.first().trimmed().toLower();
        for (const QString &param : parts) {
            const QString p = param.trimmed();
            if (p.startsWith(QStringLiteral("reason="), Qt::CaseInsensitive)) {
                entry.subscriptionReason =
                    PresenceInfo::normalizeSubscriptionReason(p.mid(7).remove(QLatin1Char('"')));
            } else if (p.startsWith(QStringLiteral("expires="), Qt::CaseInsensitive)) {
                bool ok = false;
                const int val = p.mid(8).toInt(&ok);
                if (ok)
                    entry.subscriptionExpires = val;
            }
        }
    }
    if (entry.subscriptionExpires < 0) {
        const QString expiresHeader = findHeaderValue(headerLines, QStringLiteral("Expires"));
        bool ok = false;
        const int val = expiresHeader.toInt(&ok);
        if (ok)
            entry.subscriptionExpires = val;
    }

    // Body: byte-accurate extraction (Content-Length bounded), decoded as
    // UTF-8 text — PIDF bodies are never Content-Encoding compressed by this
    // client or the SIP-Server-RTT interop target, so no deflate handling is
    // needed here (unlike MessagingDiagnosticsStore, which does handle it
    // for SIP MESSAGE bodies).
    const SipBodyExtractor::Result extraction = SipBodyExtractor::extract(trace.rawSip);
    const QString textBody = normalizeLineEndings(QString::fromUtf8(extraction.rawBodyBytes)).trimmed();

    if (!textBody.isEmpty() && entry.contentType.contains(QStringLiteral("pidf+xml"), Qt::CaseInsensitive)) {
        entry.pidf = PidfParser::parse(textBody);
    } else if (entry.method.compare(QStringLiteral("NOTIFY"), Qt::CaseInsensitive) == 0
               && textBody.isEmpty()) {
        // A body-less NOTIFY is valid per RFC 3265 (e.g. a final NOTIFY on
        // subscription termination) — not an error, just nothing to parse.
        entry.pidf.parseStatus = PresenceInfo::ParseStatus::Ok;
    }

    return entry;
}

void PresenceDiagnosticsStore::onSipMessageLogged(const SipMessageTrace &trace)
{
    if (!isPresenceRelevant(trace))
        return;

    const PresenceTraceEntry entry = buildEntry(trace);
    m_entries.append(entry);
    emit entryLogged(entry);
}

const QList<PresenceTraceEntry> &PresenceDiagnosticsStore::entries() const
{
    return m_entries;
}

void PresenceDiagnosticsStore::clear()
{
    m_entries.clear();
    emit cleared();
}
