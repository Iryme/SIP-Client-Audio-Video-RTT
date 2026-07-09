#pragma once
#include <QList>
#include <QMutex>
#include <QObject>

#include "sip/MessagingEvent.h"
#include "sip/MessagingTraceEntry.h"

// Unified, transport-independent messaging event store. Sits on top of
// MessagingDiagnosticsStore (Task W090): it does not re-parse raw SIP or
// re-implement CPIM/IMDN/is-composing/SDP-MSRP parsing itself, it only maps
// the already-parsed MessagingTraceEntry rows into transport-independent
// MessagingEvent rows so the UI and export layers can depend on one stable
// shape instead of SIP-MESSAGE/MSRP-specific types.
//
// Strictly read-only / diagnostic: never opens an MSRP socket, never starts
// an MSRP session, never sends a SIP message.
//
// The internal list is protected by a mutex so append()/clear()/snapshot()
// are safe to call from any thread; Qt signals are still expected to be
// consumed on the receiver's (GUI) thread via the normal
// auto-connection/queued-connection mechanism.
class MessagingEventStore : public QObject
{
    Q_OBJECT
public:
    static MessagingEventStore &instance();

    void append(const MessagingEvent &event);
    void clear();
    int  count() const;

    // Thread-safe copy of the current events.
    QList<MessagingEvent> snapshot() const;

    QString exportToJson() const;
    QString exportToText() const;

    int  maxEventsRetained() const;
    void setMaxEventsRetained(int max);

    // Exposed for unit testing without a live MessagingDiagnosticsStore/signal chain.
    static MessagingEvent mapFromTraceEntry(const MessagingTraceEntry &entry, qint64 id);

signals:
    void eventAppended(const MessagingEvent &event);
    void cleared();

private slots:
    void onDiagnosticsEntryLogged(const MessagingTraceEntry &entry);
    void onDiagnosticsCleared();

private:
    explicit MessagingEventStore(int maxEventsRetained = 1000);

    mutable QMutex        m_mutex;
    QList<MessagingEvent> m_events;
    qint64                m_nextId{1};
    int                   m_maxEventsRetained;
};
