#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

// One row of the simple conversational Message History (Task W093).
// Independent of MessagingEvent/MessagingTraceEntry (Task W090/W091): those
// remain the read-only diagnostics feed sourced from raw SIP trace capture;
// this is a separate, minimal "did I send/receive this" list sourced
// directly from the SIP MESSAGE composer (outbound) and the dedicated
// pjsua2 incoming-message callback (inbound), so the two pipelines never
// write duplicate rows into the same store.
struct MessageHistoryEntry
{
    enum class Direction { Inbound, Outbound };

    // Outbound-only lifecycle. Inbound rows are always conceptually
    // "received" and are rendered as such by the UI; this field stays
    // Unknown for inbound entries.
    enum class OutboundStatus { Unknown, Queued, Submitted, Sent, Failed };

    static QString directionToString(Direction d)
    {
        return d == Direction::Outbound ? QStringLiteral("outbound") : QStringLiteral("inbound");
    }

    static QString outboundStatusToString(OutboundStatus s)
    {
        switch (s) {
        case OutboundStatus::Queued:    return QStringLiteral("queued");
        case OutboundStatus::Submitted: return QStringLiteral("submitted");
        case OutboundStatus::Sent:      return QStringLiteral("sent");
        case OutboundStatus::Failed:    return QStringLiteral("failed");
        default:                        return QStringLiteral("unknown");
        }
    }

    qint64        id{0};
    QDateTime     timestamp;
    Direction     direction{Direction::Outbound};
    QString       peerUri;      // inbound: fromUri; outbound: toUri
    QString       contentType;
    QString       bodyPreview;  // capped, never the full raw body (see MessageHistoryStore)
    OutboundStatus outboundStatus{OutboundStatus::Unknown};
    QString       callId;
    QString       contactUri;   // inbound only, when the Contact header was present
    QString       profileId;    // account/profile associated, when it could be determined
};

Q_DECLARE_METATYPE(MessageHistoryEntry)
