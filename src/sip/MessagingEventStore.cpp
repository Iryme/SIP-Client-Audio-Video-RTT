#include "MessagingEventStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QTextStream>

#include "core/AppSettings.h"
#include "sip/MessagingDiagnosticsStore.h"
#include "sip/UrlRedactor.h"

namespace {

MessagingEvent::Direction mapDirection(SipMessageTrace::Direction d)
{
    return d == SipMessageTrace::Direction::Outbound
        ? MessagingEvent::Direction::Outbound
        : MessagingEvent::Direction::Inbound;
}

MessagingEvent::PayloadType mapPayloadType(MessagingContentKind kind)
{
    switch (kind) {
    case MessagingContentKind::PlainText:   return MessagingEvent::PayloadType::Plain;
    case MessagingContentKind::Html:        return MessagingEvent::PayloadType::Html;
    case MessagingContentKind::Cpim:        return MessagingEvent::PayloadType::Cpim;
    case MessagingContentKind::Imdn:        return MessagingEvent::PayloadType::Imdn;
    case MessagingContentKind::IsComposing: return MessagingEvent::PayloadType::IsComposing;
    case MessagingContentKind::Sdp:         return MessagingEvent::PayloadType::Sdp;
    case MessagingContentKind::RcsFtHttp:   return MessagingEvent::PayloadType::RcsFtHttp;
    case MessagingContentKind::Unknown:     break;
    }
    return MessagingEvent::PayloadType::Unknown;
}

} // namespace

MessagingEventStore &MessagingEventStore::instance()
{
    static MessagingEventStore s;
    return s;
}

MessagingEventStore::MessagingEventStore(int maxEventsRetained)
    : QObject(nullptr)
    , m_maxEventsRetained(maxEventsRetained > 0 ? maxEventsRetained : 1000)
{
    qRegisterMetaType<MessagingEvent>("MessagingEvent");

    m_maxEventsRetained = AppSettings::loadMaxMessagingEventsRetained();
    if (m_maxEventsRetained <= 0)
        m_maxEventsRetained = 1000;

    connect(&MessagingDiagnosticsStore::instance(), &MessagingDiagnosticsStore::entryLogged,
            this, &MessagingEventStore::onDiagnosticsEntryLogged);
    connect(&MessagingDiagnosticsStore::instance(), &MessagingDiagnosticsStore::cleared,
            this, &MessagingEventStore::onDiagnosticsCleared);
}

MessagingEvent MessagingEventStore::mapFromTraceEntry(const MessagingTraceEntry &entry, qint64 id)
{
    MessagingEvent event;
    event.id             = id;
    event.timestamp      = entry.timestamp;
    event.direction      = mapDirection(entry.direction);
    event.from           = entry.fromUri;
    event.to             = entry.toUri;
    event.callId         = entry.callId;
    event.cseq           = entry.cSeq;
    event.contentType    = entry.contentType;
    event.bodyPreview    = entry.bodyPreview;
    event.rawSipRedacted = entry.rawSip;
    event.payloadType    = mapPayloadType(entry.contentKind);

    event.contentEncoding      = entry.contentEncoding;
    event.decodeStatus         = contentDecodeStatusToString(entry.decodeStatus);
    event.decodeVariant        = DeflateDecoder::variantToString(entry.decodeVariant);
    event.compressedBodyLength = entry.compressedBodyLength;
    event.decodedBodyLength    = entry.decodedBodyLength;
    event.decodeError          = entry.decodeError;

    // IMDN Foundation (Task W096) — reuses the already-parsed ImdnInfo
    // (Task W090's ImdnParser, unchanged), no duplicated logic.
    if (entry.imdn.present) {
        event.generatedImdn = (event.direction == MessagingEvent::Direction::Outbound);
        event.receivedImdn  = (event.direction == MessagingEvent::Direction::Inbound);
        event.correlatedMessageId = entry.imdn.messageId;
        event.deliveryState = ImdnInfo::dispositionToString(entry.imdn.disposition);
    }

    if (entry.rcsFtHttp.present) {
        event.fileInfoType    = entry.rcsFtHttp.fileInfoType;
        event.fileName        = entry.rcsFtHttp.fileName;
        event.fileSize        = entry.rcsFtHttp.fileSize;
        event.fileContentType = entry.rcsFtHttp.contentType;
        event.dataUrlRedacted = UrlRedactor::redact(entry.rcsFtHttp.dataUrl);
        event.expiresAt       = entry.rcsFtHttp.expiresAt;
        event.thumbnailPresent = entry.rcsFtHttp.thumbnailPresent;
    }

    // Transport: a SIP MESSAGE request/response is transported directly over
    // SIP; an SDP offer/answer carrying an "m=message" MSRP media line is
    // diagnosing an (unopened) MSRP negotiation. Neither path starts a real
    // MSRP session — this is classification only.
    if (entry.method.compare(QStringLiteral("MESSAGE"), Qt::CaseInsensitive) == 0)
        event.transport = MessagingEvent::Transport::SipMessage;
    else if (entry.sdpMsrp.present)
        event.transport = MessagingEvent::Transport::Msrp;
    else
        event.transport = MessagingEvent::Transport::Unknown;

    event.parseStatus = MessagingEvent::ParseStatus::Ok;

    // A Content-Encoding decode failure/limit/unsupported case always takes
    // priority over the content-kind checks below: when the body couldn't
    // be decoded, buildEntry() never attempted to XML-parse it, so every
    // structured parser below will correctly report "not present" — this
    // branch gives that a precise, decode-specific warning instead of the
    // generic "no disposition could be parsed" message.
    if (entry.decodeStatus == ContentDecodeStatus::Failed
        || entry.decodeStatus == ContentDecodeStatus::LimitExceeded) {
        event.parseStatus = MessagingEvent::ParseStatus::Partial;
        event.parseWarnings << QStringLiteral("deflate decode failed");
    } else if (entry.decodeStatus == ContentDecodeStatus::Unsupported) {
        event.parseStatus = MessagingEvent::ParseStatus::Partial;
        event.parseWarnings << QStringLiteral(
            "Content-Encoding \"%1\" is not supported — body left undecoded.").arg(entry.contentEncoding);
    }

    // Surface parsing gaps as warnings instead of silently dropping them, so
    // the UI can flag partially-understood content without blocking on it.
    switch (entry.contentKind) {
    case MessagingContentKind::Cpim:
        if (!entry.cpim.present && entry.decodeStatus != ContentDecodeStatus::Failed
            && entry.decodeStatus != ContentDecodeStatus::LimitExceeded
            && entry.decodeStatus != ContentDecodeStatus::Unsupported) {
            event.parseStatus = MessagingEvent::ParseStatus::Partial;
            event.parseWarnings << QStringLiteral(
                "Content-Type declared message/cpim but no CPIM header block could be parsed.");
        }
        break;
    case MessagingContentKind::Imdn:
        if (!entry.imdn.present && entry.decodeStatus != ContentDecodeStatus::Failed
            && entry.decodeStatus != ContentDecodeStatus::LimitExceeded
            && entry.decodeStatus != ContentDecodeStatus::Unsupported) {
            event.parseStatus = MessagingEvent::ParseStatus::Partial;
            event.parseWarnings << QStringLiteral(
                "Content-Type declared message/imdn+xml but no IMDN disposition could be parsed.");
        }
        break;
    case MessagingContentKind::IsComposing:
        if (!entry.isComposing.present && entry.decodeStatus != ContentDecodeStatus::Failed
            && entry.decodeStatus != ContentDecodeStatus::LimitExceeded
            && entry.decodeStatus != ContentDecodeStatus::Unsupported) {
            event.parseStatus = MessagingEvent::ParseStatus::Partial;
            event.parseWarnings << QStringLiteral(
                "Content-Type declared application/im-iscomposing+xml but no state could be parsed.");
        }
        break;
    case MessagingContentKind::RcsFtHttp:
        if (!entry.rcsFtHttp.present && entry.decodeStatus != ContentDecodeStatus::Failed
            && entry.decodeStatus != ContentDecodeStatus::LimitExceeded
            && entry.decodeStatus != ContentDecodeStatus::Unsupported) {
            event.parseStatus = MessagingEvent::ParseStatus::Partial;
            event.parseWarnings << QStringLiteral(
                "Content-Type declared application/vnd.gsma.rcs-ft-http+xml but no file-info could be parsed.");
        }
        break;
    default:
        break;
    }

    if (entry.sdpMsrp.present && entry.sdpMsrp.transportProtocol.isEmpty()) {
        event.parseStatus = MessagingEvent::ParseStatus::Partial;
        event.parseWarnings << QStringLiteral(
            "SDP m=message line found but no MSRP transport protocol token could be parsed.");
    }

    if (event.callId.isEmpty()) {
        event.parseStatus = MessagingEvent::ParseStatus::Partial;
        event.parseWarnings << QStringLiteral("No Call-ID present on this trace.");
    }

    return event;
}

void MessagingEventStore::onDiagnosticsEntryLogged(const MessagingTraceEntry &entry)
{
    append(mapFromTraceEntry(entry, m_nextId));
}

void MessagingEventStore::onDiagnosticsCleared()
{
    clear();
}

void MessagingEventStore::append(const MessagingEvent &eventIn)
{
    MessagingEvent event = eventIn;
    {
        QMutexLocker locker(&m_mutex);
        if (event.id == 0)
            event.id = m_nextId;
        m_nextId = event.id + 1;

        m_events.append(event);
        while (m_events.size() > m_maxEventsRetained)
            m_events.removeFirst();
    }
    emit eventAppended(event);
}

void MessagingEventStore::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_events.clear();
        // Reset id numbering so it stays aligned 1:1 with
        // MessagingDiagnosticsStore's own (never-evicted) entry list, which
        // is always cleared together with this store (see
        // onDiagnosticsCleared()).
        m_nextId = 1;
    }
    emit cleared();
}

int MessagingEventStore::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_events.size();
}

QList<MessagingEvent> MessagingEventStore::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_events;
}

int MessagingEventStore::maxEventsRetained() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxEventsRetained;
}

void MessagingEventStore::setMaxEventsRetained(int max)
{
    if (max <= 0)
        return;
    QMutexLocker locker(&m_mutex);
    m_maxEventsRetained = max;
    while (m_events.size() > m_maxEventsRetained)
        m_events.removeFirst();
}

QString MessagingEventStore::exportToText() const
{
    const QList<MessagingEvent> events = snapshot();

    QString out;
    QTextStream ts(&out);
    for (const auto &e : events) {
        ts << e.timestamp.toString(Qt::ISODateWithMs)
           << QStringLiteral(" #") << e.id
           << (e.direction == MessagingEvent::Direction::Outbound
               ? QStringLiteral(" >>> ") : e.direction == MessagingEvent::Direction::Inbound
               ? QStringLiteral(" <<< ") : QStringLiteral(" --- "))
           << QStringLiteral("transport=") << MessagingEvent::transportToString(e.transport)
           << QStringLiteral(" payload=") << MessagingEvent::payloadTypeToString(e.payloadType)
           << QStringLiteral(" parseStatus=") << MessagingEvent::parseStatusToString(e.parseStatus);
        if (!e.from.isEmpty())
            ts << QStringLiteral("  From: ") << e.from;
        if (!e.to.isEmpty())
            ts << QStringLiteral("  To: ") << e.to;
        if (!e.callId.isEmpty())
            ts << QStringLiteral("  Call-ID: ") << e.callId;
        if (!e.cseq.isEmpty())
            ts << QStringLiteral("  CSeq: ") << e.cseq;
        if (!e.contentType.isEmpty())
            ts << QStringLiteral("  Content-Type: ") << e.contentType;
        ts << '\n';

        if (!e.contentEncoding.isEmpty())
            ts << QStringLiteral("  Content-Encoding: ") << e.contentEncoding
               << QStringLiteral(" decodeStatus=") << e.decodeStatus
               << QStringLiteral(" decodeVariant=") << e.decodeVariant
               << QStringLiteral(" compressed=") << e.compressedBodyLength
               << QStringLiteral(" decoded=") << e.decodedBodyLength << '\n';

        if (!e.bodyPreview.isEmpty())
            ts << QStringLiteral("  Body preview: ") << e.bodyPreview << '\n';

        if (e.generatedImdn || e.receivedImdn)
            ts << QStringLiteral("  IMDN: correlatedMessageId=") << e.correlatedMessageId
               << QStringLiteral(" deliveryState=") << e.deliveryState << '\n';

        if (e.payloadType == MessagingEvent::PayloadType::RcsFtHttp) {
            ts << QStringLiteral("  RCS file: ") << e.fileName
               << QStringLiteral("  type: ") << e.fileContentType
               << QStringLiteral("  size: ") << e.fileSize
               << QStringLiteral("  expires: ") << e.expiresAt
               << QStringLiteral("  URL: ") << e.dataUrlRedacted << '\n';
        }

        for (const QString &warning : e.parseWarnings)
            ts << QStringLiteral("  Parse warning: ") << warning << '\n';

        if (!e.rawSipRedacted.isEmpty()) {
            // rawSipRedacted is already redacted upstream (SipTraceLogger).
            ts << e.rawSipRedacted;
            if (!e.rawSipRedacted.endsWith(QLatin1Char('\n')))
                ts << '\n';
        }
        ts << QStringLiteral("--- end event ---\n");
    }
    return out;
}

QString MessagingEventStore::exportToJson() const
{
    const QList<MessagingEvent> events = snapshot();

    QJsonArray arr;
    for (const auto &e : events) {
        QJsonObject obj;
        obj[QStringLiteral("id")]          = e.id;
        obj[QStringLiteral("timestamp")]   = e.timestamp.toString(Qt::ISODateWithMs);
        obj[QStringLiteral("direction")]   = MessagingEvent::directionToString(e.direction);
        obj[QStringLiteral("transport")]   = MessagingEvent::transportToString(e.transport);
        obj[QStringLiteral("payloadType")] = MessagingEvent::payloadTypeToString(e.payloadType);
        obj[QStringLiteral("from")]        = e.from;
        obj[QStringLiteral("to")]          = e.to;
        obj[QStringLiteral("callId")]      = e.callId;
        obj[QStringLiteral("cseq")]        = e.cseq;
        obj[QStringLiteral("contentType")] = e.contentType;
        obj[QStringLiteral("bodyPreview")] = e.bodyPreview;
        obj[QStringLiteral("parseStatus")] = MessagingEvent::parseStatusToString(e.parseStatus);

        obj[QStringLiteral("contentEncoding")]      = e.contentEncoding;
        obj[QStringLiteral("decodeStatus")]         = e.decodeStatus;
        obj[QStringLiteral("decodeVariant")]        = e.decodeVariant;
        obj[QStringLiteral("compressedBodyLength")] = e.compressedBodyLength;
        obj[QStringLiteral("decodedBodyLength")]    = e.decodedBodyLength;
        if (!e.decodeError.isEmpty())
            obj[QStringLiteral("decodeError")] = e.decodeError;

        obj[QStringLiteral("generatedImdn")]      = e.generatedImdn;
        obj[QStringLiteral("receivedImdn")]       = e.receivedImdn;
        obj[QStringLiteral("correlatedMessageId")] = e.correlatedMessageId;
        obj[QStringLiteral("deliveryState")]      = e.deliveryState;

        if (e.payloadType == MessagingEvent::PayloadType::RcsFtHttp) {
            QJsonObject rcs;
            rcs[QStringLiteral("fileInfoType")]     = e.fileInfoType;
            rcs[QStringLiteral("fileName")]         = e.fileName;
            rcs[QStringLiteral("fileSize")]         = e.fileSize;
            rcs[QStringLiteral("contentType")]      = e.fileContentType;
            rcs[QStringLiteral("dataUrlRedacted")]  = e.dataUrlRedacted;
            rcs[QStringLiteral("expiresAt")]        = e.expiresAt;
            rcs[QStringLiteral("thumbnailPresent")] = e.thumbnailPresent;
            obj[QStringLiteral("rcsFileTransfer")] = rcs;
        }

        QJsonArray warnings;
        for (const QString &warning : e.parseWarnings)
            warnings.append(warning);
        obj[QStringLiteral("parseWarnings")] = warnings;

        // rawSipRedacted is already redacted upstream (SipTraceLogger) —
        // safe to export as-is.
        obj[QStringLiteral("rawSipRedacted")] = e.rawSipRedacted;

        arr.append(obj);
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Indented);
}
