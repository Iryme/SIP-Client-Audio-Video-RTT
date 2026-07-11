#include "InteropTraceExporter.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "sip/MessagingDiagnosticsStore.h"
#include "sip/MessagingEvent.h"
#include "sip/MessagingEventStore.h"
#include "sip/PresenceDiagnosticsStore.h"
#include "sip/UrlRedactor.h"

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
    // Task W095: literal "parseStatus" alias of "status" above, plus the
    // parse warnings that "status" alone doesn't carry — kept as an
    // additional field rather than a rename to stay compatible with the
    // v1 schema's "status" contract (see InteropTraceExporter.h).
    obj[QStringLiteral("parseStatus")] = MessagingEvent::parseStatusToString(ev.parseStatus);
    QJsonArray parseWarnings;
    for (const QString &warning : ev.parseWarnings)
        parseWarnings.append(warning);
    obj[QStringLiteral("parseWarnings")] = parseWarnings;
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

    // Content-Encoding decode diagnostics (Task W095).
    obj[QStringLiteral("contentEncoding")]      = entry.contentEncoding;
    obj[QStringLiteral("decodedBodyPreview")]   = entry.decodedBodyPreview;
    obj[QStringLiteral("decodeStatus")]         = contentDecodeStatusToString(entry.decodeStatus);
    obj[QStringLiteral("decodeVariant")]        = DeflateDecoder::variantToString(entry.decodeVariant);
    obj[QStringLiteral("compressedBodyLength")] = entry.compressedBodyLength;
    obj[QStringLiteral("decodedBodyLength")]    = entry.decodedBodyLength;
    if (!entry.decodeError.isEmpty())
        obj[QStringLiteral("decodeError")] = entry.decodeError;

    // IMDN Foundation (Task W096) — additive fields, schemaVersion stays 2
    // (kSchemaVersion). generatedImdn/receivedImdn/correlatedMessageId/
    // deliveryState mirror MessagingEventStore::mapFromTraceEntry (Task
    // W091's classification), computed once above via `ev`, not re-derived.
    obj[QStringLiteral("generatedImdn")]       = ev.generatedImdn;
    obj[QStringLiteral("receivedImdn")]        = ev.receivedImdn;
    obj[QStringLiteral("correlatedMessageId")] = ev.correlatedMessageId;
    obj[QStringLiteral("deliveryState")]       = ev.deliveryState;

    // is-composing (Task W097) — additive fields, schemaVersion stays 2.
    obj[QStringLiteral("generatedIsComposing")] = ev.generatedIsComposing;
    obj[QStringLiteral("receivedIsComposing")]  = ev.receivedIsComposing;
    obj[QStringLiteral("typingState")]          = ev.typingState;
    obj[QStringLiteral("typingRefresh")]        = ev.typingRefresh;
    obj[QStringLiteral("typingTimeout")]        = ev.typingTimeout;

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

    if (entry.rcsFtHttp.present) {
        QJsonObject rcs;
        rcs[QStringLiteral("fileInfoType")] = entry.rcsFtHttp.fileInfoType;
        rcs[QStringLiteral("fileName")]     = entry.rcsFtHttp.fileName;
        rcs[QStringLiteral("fileSize")]     = entry.rcsFtHttp.fileSize;
        rcs[QStringLiteral("contentType")]  = entry.rcsFtHttp.contentType;
        // The full URL (entry.rcsFtHttp.dataUrl) commonly carries a one-time
        // download token — only the redacted form is ever exported; see
        // docs/content-encoding-diagnostics.md / docs/rcs-ft-http-diagnostics.md.
        rcs[QStringLiteral("dataUrlRedacted")]  = UrlRedactor::redact(entry.rcsFtHttp.dataUrl);
        rcs[QStringLiteral("expiresAt")]        = entry.rcsFtHttp.expiresAt;
        rcs[QStringLiteral("thumbnailPresent")] = entry.rcsFtHttp.thumbnailPresent;
        obj[QStringLiteral("rcsFileTransfer")] = rcs;
    }

    return obj;
}

// SIP Presence (Task W098): built directly from PresenceTraceEntry
// (SUBSCRIBE/NOTIFY raw-trace diagnostics, see PresenceDiagnosticsStore) —
// entirely independent of MessagingTraceEntry/MessagingEvent, per the
// requirement to keep Presence diagnostics out of the Messaging pipeline.
QJsonObject buildPresenceEvent(const PresenceTraceEntry &entry, qint64 eventId)
{
    QJsonObject obj;
    obj[QStringLiteral("eventId")]   = static_cast<double>(eventId);
    obj[QStringLiteral("timestamp")] = entry.timestamp.toString(Qt::ISODateWithMs);
    obj[QStringLiteral("direction")] = entry.direction == SipMessageTrace::Direction::Outbound
        ? QStringLiteral("outbound") : QStringLiteral("inbound");
    obj[QStringLiteral("method")]              = entry.method;
    obj[QStringLiteral("callId")]              = entry.callId;
    obj[QStringLiteral("cseq")]                = entry.cSeq;
    obj[QStringLiteral("from")]                = entry.fromUri;
    obj[QStringLiteral("to")]                  = entry.toUri;
    obj[QStringLiteral("eventPackage")]        = entry.eventPackage;
    obj[QStringLiteral("subscriptionState")]   = entry.subscriptionState;
    obj[QStringLiteral("subscriptionExpires")] = entry.subscriptionExpires;
    obj[QStringLiteral("subscriptionReason")]  = entry.subscriptionReason;
    obj[QStringLiteral("contentType")]         = entry.contentType;
    obj[QStringLiteral("parseStatus")]         = PresenceInfo::parseStatusToString(entry.pidf.parseStatus);

    QJsonArray warnings;
    for (const QString &warning : entry.pidf.parseWarnings)
        warnings.append(warning);
    obj[QStringLiteral("parseWarnings")] = warnings;

    // Already redacted upstream by SipTraceLogger — safe to export as-is.
    obj[QStringLiteral("rawSipRedacted")] = entry.rawSip;

    QJsonObject presence;
    presence[QStringLiteral("entity")]         = entry.pidf.entityUri;
    presence[QStringLiteral("tupleId")]        = entry.pidf.tupleId;
    presence[QStringLiteral("basicStatus")]    = PresenceInfo::basicStatusToString(entry.pidf.basicStatus);
    presence[QStringLiteral("extendedStatus")] = PresenceInfo::extendedStatusToString(entry.pidf.extendedStatus);
    presence[QStringLiteral("contact")]        = entry.pidf.contactUri;
    presence[QStringLiteral("priority")]       = entry.pidf.priority;
    presence[QStringLiteral("note")]           = entry.pidf.note;
    presence[QStringLiteral("timestamp")]      = entry.pidf.timestamp.isValid()
        ? entry.pidf.timestamp.toString(Qt::ISODateWithMs) : QString();
    obj[QStringLiteral("presence")] = presence;

    return obj;
}

} // namespace

QString exportToJson(const QList<MessagingTraceEntry> &entries,
                      const QList<PresenceTraceEntry> &presenceEntries)
{
    QJsonArray events;
    qint64 id = 1;
    for (const MessagingTraceEntry &entry : entries)
        events.append(buildEvent(entry, id++));

    QJsonArray presenceEvents;
    qint64 presenceId = 1;
    for (const PresenceTraceEntry &entry : presenceEntries)
        presenceEvents.append(buildPresenceEvent(entry, presenceId++));

    QJsonObject root;
    root[QStringLiteral("schemaVersion")]  = kSchemaVersion;
    root[QStringLiteral("source")]         = QStringLiteral("windows-client");
    root[QStringLiteral("exportedAt")]     = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    root[QStringLiteral("events")]         = events;
    root[QStringLiteral("presenceEvents")] = presenceEvents;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString exportToJson()
{
    return exportToJson(MessagingDiagnosticsStore::instance().entries(),
                        PresenceDiagnosticsStore::instance().entries());
}

} // namespace InteropTraceExporter
