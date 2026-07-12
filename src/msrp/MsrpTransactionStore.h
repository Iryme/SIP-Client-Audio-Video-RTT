#pragma once
#include <QList>
#include <QObject>

#include "msrp/MsrpTransaction.h"

// Tracks in-flight and recently-completed MSRP transactions (Task W100,
// section Q). Thread-confined to the Qt/UI thread (MsrpSession only ever
// calls it from Qt event-loop-driven socket signal handlers, never from a
// worker thread — see docs/msrp-transport.md).
class MsrpTransactionStore : public QObject
{
    Q_OBJECT
public:
    explicit MsrpTransactionStore(QObject *parent = nullptr);

    MsrpTransaction begin(const QString &transactionId, const QString &sessionKey,
                          MsrpMethod method, const QString &messageId, bool outbound);
    bool updateStatus(const QString &transactionId, MsrpTransactionStatus status,
                      int responseCode = 0, const QString &responseComment = QString());
    bool applyReport(const QString &transactionId, const QString &reportStatus, bool success);

    QList<MsrpTransaction> pendingForSession(const QString &sessionKey) const;
    QList<MsrpTransaction> all() const;
    int pendingCount(const QString &sessionKey) const;

    void clear();
    // Bounds memory: completed transactions beyond this count are evicted
    // (oldest first); pending transactions are never evicted this way.
    static constexpr int kMaxRetainedCompleted = 1000;

signals:
    void transactionUpdated(const MsrpTransaction &transaction);
    void cleared();

private:
    void trimCompleted();

    QList<MsrpTransaction> m_transactions; // pending + a bounded completed tail
};
