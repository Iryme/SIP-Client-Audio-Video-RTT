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

    // IMDN-aware delivery state (Task W096). Outbound entries: starts None,
    // moves to Delivered/Displayed/Failed/Error only once a correlated IMDN
    // report is received for this entry's messageId (never inferred from
    // outboundStatus alone — a 200 OK on the MESSAGE request is not an
    // IMDN). Inbound entries: stays None (IMDN reports we receive apply to
    // an *outbound* entry, not to themselves).
    enum class DeliveryState { None, Delivered, Displayed, Failed, Error };

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

    static QString deliveryStateToString(DeliveryState s)
    {
        switch (s) {
        case DeliveryState::Delivered: return QStringLiteral("delivered");
        case DeliveryState::Displayed: return QStringLiteral("displayed");
        case DeliveryState::Failed:    return QStringLiteral("failed");
        case DeliveryState::Error:     return QStringLiteral("error");
        default:                       return QStringLiteral("none");
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

    // IMDN Foundation (Task W096) — Message-ID correlation.
    // Outbound plain message: the Message-ID we generated (only when IMDN
    // was requested — see SipMessageComposer::compose). Inbound plain
    // message: the Message-ID header the sender included, if any.
    QString       messageId;
    // Set only on entries whose body IS an IMDN report itself (outbound:
    // one we generated via composeImdnReport; inbound: one we received).
    // correlatedMessageId is the original message's Message-ID that report
    // is about.
    bool          isImdnReport{false};
    QString       correlatedMessageId;

    // Outbound entries only: richer state once a correlated IMDN report
    // arrives (see DeliveryState above). Inbound entries: always None.
    DeliveryState deliveryState{DeliveryState::None};

    // Inbound plain-message entries only: whether the sender's
    // Disposition-Notification header requested each report, and whether
    // this client has already sent that report back (never sent twice).
    bool          deliveryNotificationRequested{false};
    bool          displayNotificationRequested{false};
    bool          deliveredImdnSent{false};
    bool          displayedImdnSent{false};

    // is-composing (Task W097). Set only on entries whose body IS an
    // RFC 3994 typing notification (always inbound — this client's own
    // outbound typing notifications are driven by TypingIndicatorController
    // and never written into Message History, only the peer's are).
    bool          isTypingNotification{false};
    QString       typingState; // "active" / "idle" / "gone"

    // Task W111 (Client Messaging View): outbound entries only. The actual
    // transport MessagingTransportPolicy/SipManager::sendSipMessage used for
    // this specific send, set once the transport decision is known (empty
    // until then — appendOutbound() runs before the decision is made).
    // messagingActualTransportToString() values ("sip-message"/"msrp"/
    // "sip-message-fallback"), or empty if never resolved (e.g. rejected
    // before a transport was chosen).
    QString       actualTransport;
    // Set only when actualTransport == "sip-message-fallback": why MSRP was
    // not used (mirrors MessagingTransportPolicy::Decision::reason).
    QString       fallbackReason;

    // Task W111: MSRP-specific delivery correlation. MSRP Message-IDs are a
    // distinct identifier space from the SIP MESSAGE Message-ID above
    // (messageId) — never conflated. Only ever set on outbound entries that
    // were actually sent via MSRP (actualTransport == "msrp").
    QString       msrpMessageId;
};

Q_DECLARE_METATYPE(MessageHistoryEntry)
