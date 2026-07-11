#pragma once
#include <QList>
#include <QObject>

#include "sip/PresenceTraceEntry.h"
#include "sip/SipMessageTrace.h"

// Presence diagnostics trace store (Task W098). Subscribes to SipTraceLogger
// (the same raw SIP capture path used by the SIP Ladder and by
// MessagingDiagnosticsStore) and, for every SUBSCRIBE/NOTIFY trace, builds a
// PresenceTraceEntry with the Event/Subscription-State/Expires headers and
// PIDF body parsed out via PidfParser.
//
// Strictly read-only / diagnostic: never initiates a subscription itself
// (that is SipAccount/SipManager's job) and never writes into PresenceStore
// — PresenceStore's canonical current-state is fed only by the live
// pjsua2-Buddy pipeline, so there is exactly one writer per store (no two
// producers racing to update the same entity).
class PresenceDiagnosticsStore : public QObject
{
    Q_OBJECT
public:
    static PresenceDiagnosticsStore &instance();

    const QList<PresenceTraceEntry> &entries() const;
    void clear();

    // Exposed for unit testing without a live SipTraceLogger/signal chain.
    static bool isPresenceRelevant(const SipMessageTrace &trace);
    static PresenceTraceEntry buildEntry(const SipMessageTrace &trace);

signals:
    void entryLogged(const PresenceTraceEntry &entry);
    void cleared();

private slots:
    void onSipMessageLogged(const SipMessageTrace &trace);

private:
    PresenceDiagnosticsStore();

    QList<PresenceTraceEntry> m_entries;
};
