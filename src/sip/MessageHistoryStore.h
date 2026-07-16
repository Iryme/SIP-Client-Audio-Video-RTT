#pragma once
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>

#include "msrp/MsrpTypes.h"
#include "sip/ImdnInfo.h"
#include "sip/MessageHistoryEntry.h"
#include "sip/SipMessageComposer.h"

// Simple conversational Message History store (Task W093).
//
// Deliberately separate from MessagingEventStore (Task W091): that store
// stays exclusively fed by the existing read-only SIP trace-capture
// pipeline (SipTraceLogger -> MessagingDiagnosticsStore -> MessagingEventStore,
// unchanged since W090), which remains the source for the SIP Ladder and
// Messaging Diagnostics feed. This store is fed directly by the send path
// (SipManager::sendSipMessage, outbound) and the dedicated pjsua2 incoming-
// message callback (SipAccount::instantMessageReceived, inbound) — never by
// re-reading MessagingEventStore or re-parsing raw SIP text. Keeping the two
// stores' inputs disjoint is what avoids the "obvious duplicate" the task
// warns about: the same inbound wire message is captured once by each
// pipeline, but each pipeline only ever writes to its own store.
//
// Thread-safe: the internal list is protected by a mutex, same pattern as
// MessagingEventStore.
class MessageHistoryStore : public QObject
{
    Q_OBJECT
public:
    static MessageHistoryStore &instance();

    // Appends an inbound entry from the pjsua2 onInstantMessage callback.
    // Applies a short-window fingerprint dedup (see cpp) so a retransmitted
    // or otherwise duplicated callback invocation for the same message does
    // not produce two history rows. Returns the appended entry's id, or the
    // id of the existing (deduplicated) entry if this call was a duplicate.
    // messageId/dispositionNotification are the RFC 5438 Message-ID and
    // Disposition-Notification header values (Task W096), empty when the
    // sender did not include them.
    qint64 appendInbound(const QString &fromUri, const QString &toUri,
                         const QString &contactUri, const QString &contentType,
                         const QString &body, const QString &callId,
                         const QString &profileId,
                         const QString &messageId = QString(),
                         const QString &dispositionNotification = QString());

    // Appends an inbound IMDN report (message/imdn+xml) as its own history
    // row (Task W096) — same dedup mechanism as appendInbound. Does NOT
    // itself update any other entry's deliveryState; call correlateDelivery
    // separately once the report has been parsed.
    qint64 appendInboundImdn(const QString &fromUri, const QString &toUri,
                             const QString &contactUri, const QString &body,
                             const QString &callId, const QString &profileId,
                             const QString &correlatedMessageId);

    // Appends an inbound RFC 3994 is-composing notification (Task W097) as
    // its own history row — same dedup mechanism as appendInbound. state is
    // "active"/"idle"/"gone" (IsComposingInfo::stateToString).
    qint64 appendInboundTyping(const QString &fromUri, const QString &toUri,
                               const QString &contactUri, const QString &body,
                               const QString &callId, const QString &profileId,
                               const QString &state);

    // Appends an outbound entry at send time (status Queued), from an
    // already-composed message. Returns the new entry's id.
    qint64 appendOutbound(const ComposedSipMessage &msg);

    // Updates an outbound entry's lifecycle status in place (Queued ->
    // Submitted -> Sent/Failed). No-op if id is not found (e.g. evicted).
    void updateOutboundStatus(qint64 id, MessageHistoryEntry::OutboundStatus status);

    // Task W096: finds the most recent outbound (non-IMDN-report) entry
    // whose messageId matches, and upgrades its deliveryState. No-op
    // (idempotent) if no such entry is found — e.g. the entry was evicted,
    // or the correlated Message-ID was never one we generated.
    void correlateDelivery(const QString &messageId, MessageHistoryEntry::DeliveryState state);

    // Task W111 (Client Messaging View): records which transport
    // MessagingTransportPolicy/SipManager::sendSipMessage actually used for
    // the most recently appended outbound entry with this id — called right
    // after the transport decision is made, since appendOutbound() runs
    // before that decision exists. msrpMessageId is set only when transport
    // is Msrp (a distinct id space from the SIP Message-ID). fallbackReason
    // is only meaningful when transport == SipMessageFallback.
    void updateTransportOutcome(qint64 id, MessagingActualTransport transport,
                                const QString &msrpMessageId = QString(),
                                const QString &fallbackReason = QString());

    // Task W111: correlates an MSRP SEND response / REPORT
    // (SipCall::msrpDeliveryStatusChanged) back to the outbound entry that
    // was sent with this msrpMessageId (set via updateTransportOutcome
    // above). Distinct correlation key space from correlateDelivery(), which
    // matches on the SIP MESSAGE Message-ID header instead. No-op if no
    // matching entry is found (e.g. evicted).
    void correlateMsrpDelivery(const QString &msrpMessageId,
                               MessageHistoryEntry::DeliveryState state);

    // Task W096: marks that this client has sent a delivered/displayed IMDN
    // report for the given inbound entry, so it is never sent twice.
    void markImdnSent(qint64 inboundEntryId, ImdnInfo::Disposition disposition);

    // Task W096: single-entry lookup, used by the "mark as read" UI action
    // to check displayNotificationRequested/displayedImdnSent before
    // sending a Displayed report. Returns a default-constructed (id == 0)
    // entry if not found.
    MessageHistoryEntry entryById(qint64 id) const;

    void clear();
    int  count() const;
    QList<MessageHistoryEntry> snapshot() const;

    int  maxEntriesRetained() const;
    void setMaxEntriesRetained(int max);

signals:
    void entryAppended(const MessageHistoryEntry &entry);
    void entryUpdated(const MessageHistoryEntry &entry);
    void cleared();

private:
    explicit MessageHistoryStore(int maxEntriesRetained = 1000);

    static QString makePreview(const QString &body, int maxLen = 200);
    static QString inboundFingerprint(const QString &fromUri, const QString &toUri,
                                      const QString &contentType, const QString &body,
                                      const QString &callId);

    mutable QMutex               m_mutex;
    QList<MessageHistoryEntry>   m_entries;
    QHash<QString, qint64>       m_recentInboundFingerprints; // fingerprint -> epoch ms seen
    QHash<QString, qint64>       m_recentInboundFingerprintToEntryId;
    qint64                       m_nextId{1};
    int                          m_maxEntriesRetained;
};
