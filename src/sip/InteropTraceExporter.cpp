#include "InteropTraceExporter.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEvent.h"
#include "sip/MessagingEventStore.h"

namespace InteropTraceExporter {

namespace {

QJsonObject buildEvent(const MessagingTraceEntry &entry, qint64 eventId)
{
    // Reuses MessagingEventStore's existing transport/payloadType/parseStatus
    // classification (Task W091) instead of re-deriving it here — no
    // duplicated parsing/classification logic.
    const MessagingEvent ev = MessagingEventStore::mapFromTraceEntry(entry, eventId);

    QJsonObject obj;
    obj[QStringLiteral("eventId")]     = static_cast<double>(eventId);
    obj[QStringLiteral("timestamp")]   = entry.timestamp.toString(Qt::ISODateWithMs);
    obj[QStringLiteral("direction")]   = MessagingEvent::directionToString(ev.direction);
    // "status" = parse/classification status (ok/partial/error) for this
    // event, i.e. whether the diagnostics pipeline could fully interpret it
    // — the closest existing concept to a generic per-event "status" field.
    // Not to be confused with Task W093's outbound send-status (queued/
    // submitted/sent/failed), which belongs to a live send action, not a
    // captured/replayed trace.
    obj[QStringLiteral("status")]      = MessagingEvent::parseStatusToString(ev.parseStatus);
    obj[QStringLiteral("transport")]   = MessagingEvent::transportToString(ev.transport);
    obj[QStringLiteral("payloadType")] = MessagingEvent::payloadTypeToString(ev.payloadType);
    obj[QStringLiteral("callId")]      = entry.callId;
    obj[QStringLiteral("cseq")]        = entry.cSeq;
    obj[QStringLiteral("from")]        = entry.fromUri;
    obj[QStringLiteral("to")]          = entry.toUri;
    obj[QStringLiteral("contentType")] = entry.contentType;
    obj[QStringLiteral("bodyPreview")] = entry.bodyPreview;
    // Already redacted (Authorization/Proxy-Authorization -> "[REDACTED]")
    // by SipTraceLogger::redactCredentials before this entry was ever built;
    // this exporter does not perform or need any redaction of its own.
    obj[QStringLiteral("rawSipRedacted")] = entry.rawSip;

    if (entry.imdn.present) {
        // Top-level shortcut, mirrored inside the nested "imdn" object.
        obj[QStringLiteral("messageId")] = entry.imdn.messageId;

        QJsonObject imdn;
        imdn[QStringLiteral("messageId")]         = entry.imdn.messageId;
        imdn[QStringLiteral("originalRecipient")] = entry.imdn.originalRecipient;
        imdn[QStringLiteral("finalRecipient")]    = entry.imdn.finalRecipient;
        imdn[QStringLiteral("disposition")]       = ImdnInfo::dispositionToString(entry.imdn.disposition);
        obj[QStringLiteral("imdn")] = imdn;
    }

    if (entry.cpim.present) {
        QJsonObject cpim;
        cpim[QStringLiteral("from")]        = entry.cpim.from;
        cpim[QStringLiteral("to")]          = entry.cpim.to;
        cpim[QStringLiteral("dateTime")]    = entry.cpim.dateTime;
        cpim[QStringLiteral("subject")]     = entry.cpim.subject;
        cpim[QStringLiteral("contentType")] = entry.cpim.contentType;
        obj[QStringLiteral("cpim")] = cpim;
    }

    if (entry.isComposing.present) {
        QJsonObject ic;
        ic[QStringLiteral("state")]   = IsComposingInfo::stateToString(entry.isComposing.state);
        ic[QStringLiteral("timeout")] = entry.isComposing.timeout;
        ic[QStringLiteral("refresh")] = entry.isComposing.refresh;
        obj[QStringLiteral("isComposing")] = ic;
    }

    if (entry.sdpMsrp.present) {
        QJsonObject msrp;
        msrp[QStringLiteral("sessionId")]         = entry.sdpMsrp.sessionId;
        // Raw "m=message ..." SDP line and the a=* attribute values it was
        // parsed from (Task W090's SdpMsrpDiagnosticsParser, unchanged).
        msrp[QStringLiteral("mediaLine")]         = entry.sdpMsrp.mediaLine;
        msrp[QStringLiteral("path")]              = entry.sdpMsrp.path;         // a=path
        msrp[QStringLiteral("acceptTypes")]       = entry.sdpMsrp.acceptTypes;  // a=accept-types
        msrp[QStringLiteral("setup")]             = entry.sdpMsrp.setup;        // a=setup
        msrp[QStringLiteral("connection")]        = entry.sdpMsrp.connection;   // a=connection
        msrp[QStringLiteral("transportProtocol")] = entry.sdpMsrp.transportProtocol; // TCP/MSRP or TCP/TLS/MSRP
        // transactionId and per-chunk From-Path/To-Path headers are
        // intentionally omitted: this client only ever performs SDP-level
        // MSRP diagnostics (an offer/answer was observed) and never opens a
        // real MSRP session, so there is no MSRP transaction and no MSRP
        // chunk headers to report — only the SDP a=path attribute above.
        obj[QStringLiteral("msrp")] = msrp;
    }

    return obj;
}

} // namespace

QString exportToJson(const QList<MessagingTraceEntry> &entries)
{
    QJsonArray events;
    qint64 id = 1;
    for (const MessagingTraceEntry &entry : entries)
        events.append(buildEvent(entry, id++));

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = kSchemaVersion;
    root[QStringLiteral("source")]        = QStringLiteral("windows-client");
    root[QStringLiteral("exportedAt")]    = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    root[QStringLiteral("events")]        = events;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString exportToJson()
{
    return exportToJson(MessagingDiagnosticsStore::instance().entries());
}

} // namespace InteropTraceExporter
