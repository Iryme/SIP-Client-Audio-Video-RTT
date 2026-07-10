#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

// Transport-independent messaging event. This is the model the UI/export
// layer should depend on instead of transport-specific types (SIP MESSAGE
// traces, future MSRP chunks, etc.). W090's SIP-MESSAGE/CPIM/IMDN/
// is-composing/SDP-MSRP diagnostics are mapped into this shape by
// MessagingEventStore; nothing here sends or starts any real messaging
// transport.
struct MessagingEvent
{
    enum class Direction { Inbound, Outbound, Unknown };
    enum class Transport { SipMessage, Msrp, Unknown };
    enum class PayloadType { Plain, Html, Cpim, Imdn, IsComposing, Sdp, RcsFtHttp, Unknown };
    enum class ParseStatus { Ok, Partial, Error };

    static QString directionToString(Direction d)
    {
        switch (d) {
        case Direction::Inbound:  return QStringLiteral("inbound");
        case Direction::Outbound: return QStringLiteral("outbound");
        default:                  return QStringLiteral("unknown");
        }
    }

    static QString transportToString(Transport t)
    {
        switch (t) {
        case Transport::SipMessage: return QStringLiteral("sip-message");
        case Transport::Msrp:       return QStringLiteral("msrp");
        default:                    return QStringLiteral("unknown");
        }
    }

    static QString payloadTypeToString(PayloadType p)
    {
        switch (p) {
        case PayloadType::Plain:       return QStringLiteral("plain");
        case PayloadType::Html:        return QStringLiteral("html");
        case PayloadType::Cpim:        return QStringLiteral("cpim");
        case PayloadType::Imdn:        return QStringLiteral("imdn");
        case PayloadType::IsComposing: return QStringLiteral("is-composing");
        case PayloadType::Sdp:         return QStringLiteral("sdp");
        case PayloadType::RcsFtHttp:   return QStringLiteral("rcs-ft-http");
        default:                       return QStringLiteral("unknown");
        }
    }

    static QString parseStatusToString(ParseStatus s)
    {
        switch (s) {
        case ParseStatus::Ok:      return QStringLiteral("ok");
        case ParseStatus::Partial: return QStringLiteral("partial");
        default:                   return QStringLiteral("error");
        }
    }

    qint64      id{0}; // internal unique id, assigned by MessagingEventStore
    QDateTime   timestamp;
    Direction   direction{Direction::Unknown};
    Transport   transport{Transport::Unknown};
    PayloadType payloadType{PayloadType::Unknown};

    QString from;
    QString to;
    QString callId;
    QString cseq;
    QString contentType;
    QString bodyPreview;
    QString rawSipRedacted;

    ParseStatus parseStatus{ParseStatus::Ok};
    QStringList parseWarnings;

    // Content-Encoding diagnostics (Task W095).
    QString contentEncoding;
    QString decodeStatus;  // not-needed/decoded/failed/unsupported/limit-exceeded
    QString decodeVariant; // zlib/raw-deflate/gzip/none
    qint64  compressedBodyLength{0};
    qint64  decodedBodyLength{0};
    QString decodeError; // only set when relevant

    // RCS FT HTTP diagnostics (Task W095) — populated only when
    // payloadType == RcsFtHttp; diagnostic-only, the URL is never resolved.
    QString fileInfoType;
    QString fileName;
    qint64  fileSize{-1};
    QString fileContentType;
    QString dataUrlRedacted;
    QString expiresAt;
    bool    thumbnailPresent{false};
};

Q_DECLARE_METATYPE(MessagingEvent)
