#include "MsrpTransactionStore.h"

namespace {
bool isTerminal(MsrpTransactionStatus s)
{
    return s == MsrpTransactionStatus::Accepted
        || s == MsrpTransactionStatus::ReportedSuccess
        || s == MsrpTransactionStatus::ReportedFailure
        || s == MsrpTransactionStatus::TimedOut
        || s == MsrpTransactionStatus::Aborted
        || s == MsrpTransactionStatus::Failed;
}
} // namespace

MsrpTransactionStore::MsrpTransactionStore(QObject *parent) : QObject(parent) {}

MsrpTransaction MsrpTransactionStore::begin(const QString &transactionId, const QString &sessionKey,
                                            MsrpMethod method, const QString &messageId, bool outbound)
{
    MsrpTransaction t;
    t.transactionId = transactionId;
    t.sessionKey = sessionKey;
    t.method = method;
    t.messageId = messageId;
    t.outbound = outbound;
    t.status = MsrpTransactionStatus::Queued;
    t.createdAt = QDateTime::currentDateTimeUtc();
    t.updatedAt = t.createdAt;
    m_transactions.append(t);
    emit transactionUpdated(t);
    return t;
}

bool MsrpTransactionStore::updateStatus(const QString &transactionId, MsrpTransactionStatus status,
                                        int responseCode, const QString &responseComment)
{
    for (auto &t : m_transactions) {
        if (t.transactionId == transactionId) {
            t.status = status;
            if (responseCode != 0)
                t.responseCode = responseCode;
            if (!responseComment.isEmpty())
                t.responseComment = responseComment;
            t.updatedAt = QDateTime::currentDateTimeUtc();
            emit transactionUpdated(t);
            trimCompleted();
            return true;
        }
    }
    return false;
}

bool MsrpTransactionStore::applyReport(const QString &transactionId, const QString &reportStatus, bool success)
{
    for (auto &t : m_transactions) {
        if (t.transactionId == transactionId) {
            t.reportStatus = reportStatus;
            t.status = success ? MsrpTransactionStatus::ReportedSuccess : MsrpTransactionStatus::ReportedFailure;
            t.updatedAt = QDateTime::currentDateTimeUtc();
            emit transactionUpdated(t);
            trimCompleted();
            return true;
        }
    }
    return false;
}

QList<MsrpTransaction> MsrpTransactionStore::pendingForSession(const QString &sessionKey) const
{
    QList<MsrpTransaction> result;
    for (const auto &t : m_transactions) {
        if (t.sessionKey == sessionKey && !isTerminal(t.status))
            result.append(t);
    }
    return result;
}

QList<MsrpTransaction> MsrpTransactionStore::all() const
{
    return m_transactions;
}

int MsrpTransactionStore::pendingCount(const QString &sessionKey) const
{
    return pendingForSession(sessionKey).size();
}

void MsrpTransactionStore::clear()
{
    m_transactions.clear();
    emit cleared();
}

void MsrpTransactionStore::trimCompleted()
{
    int completedCount = 0;
    for (const auto &t : m_transactions)
        if (isTerminal(t.status))
            ++completedCount;

    while (completedCount > kMaxRetainedCompleted) {
        for (int i = 0; i < m_transactions.size(); ++i) {
            if (isTerminal(m_transactions.at(i).status)) {
                m_transactions.removeAt(i);
                --completedCount;
                break;
            }
        }
    }
}
