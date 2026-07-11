#pragma once
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>

#include "sip/PresenceInfo.h"

// Current-state Presence store (Task W098 requirement 8): one canonical
// PresenceInfo per watched entity URI, plus a bounded history of updates.
// Fed exclusively by the live subscription pipeline (SipAccount's pjsua2
// Buddy callbacks, routed through SipManager) — deliberately NOT fed from
// the raw-trace diagnostics pipeline (PresenceDiagnosticsStore) to avoid two
// producers writing conflicting state for the same entity. Kept fully
// separate from MessageHistoryStore/MessagingEventStore (never mixed with
// SIP MESSAGE/IMDN/is-composing state).
//
// The internal maps are protected by a mutex so upsert()/clear()/snapshot()
// are safe to call from any thread; Qt signals are still expected to be
// consumed on the receiver's (GUI) thread via the normal auto-connection/
// queued-connection mechanism.
class PresenceStore : public QObject
{
    Q_OBJECT
public:
    static PresenceStore &instance();

    // Updates the current state for info.entityUri and appends it to the
    // bounded history. No-op (returns without emitting) if entityUri is
    // empty.
    void upsert(const PresenceInfo &info);

    // Current state for entityUri, or a default-constructed (Unknown)
    // PresenceInfo if never seen.
    PresenceInfo current(const QString &entityUri) const;

    // Thread-safe copy of all current per-entity states.
    QList<PresenceInfo> snapshot() const;

    // Thread-safe copy of the bounded update history (oldest first).
    QList<PresenceInfo> history() const;

    void clear();

    int  maxRetainedEvents() const;
    void setMaxRetainedEvents(int max);

signals:
    void presenceUpdated(const PresenceInfo &info);
    void cleared();

private:
    explicit PresenceStore(int maxRetainedEvents = 500);

    mutable QMutex             m_mutex;
    QHash<QString, PresenceInfo> m_current;
    QList<PresenceInfo>          m_history;
    int                           m_maxRetainedEvents;
};
