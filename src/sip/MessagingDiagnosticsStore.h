#pragma once
#include <QList>
#include <QObject>

#include "sip/MessagingTraceEntry.h"
#include "sip/SipMessageTrace.h"

// Messaging Diagnostics trace store. Subscribes to SipTraceLogger (the same
// raw SIP capture path used by the SIP Ladder) and, for every trace that is
// messaging-related (SIP MESSAGE requests/responses, or any body carrying
// CPIM/IMDN/is-composing/SDP m=message content), builds a MessagingTraceEntry
// with the structured diagnostics parsed out.
//
// Strictly read-only / diagnostic: this class never opens an MSRP socket,
// never starts an MSRP session, and never sends a SIP message. It only
// observes traces already captured elsewhere.
class MessagingDiagnosticsStore : public QObject
{
    Q_OBJECT
public:
    static MessagingDiagnosticsStore &instance();

    const QList<MessagingTraceEntry> &entries() const;
    void clear();

    QString exportToText() const;
    QString exportToJson() const;

    // Exposed for unit testing without a live SipTraceLogger/signal chain.
    static bool isMessagingRelevant(const SipMessageTrace &trace);
    static MessagingTraceEntry buildEntry(const SipMessageTrace &trace);

signals:
    void entryLogged(const MessagingTraceEntry &entry);
    void cleared();

private slots:
    void onSipMessageLogged(const SipMessageTrace &trace);

private:
    MessagingDiagnosticsStore();

    QList<MessagingTraceEntry> m_entries;
};
