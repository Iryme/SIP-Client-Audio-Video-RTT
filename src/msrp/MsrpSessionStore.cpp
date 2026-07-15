#include "MsrpSessionStore.h"

MsrpSessionStore &MsrpSessionStore::instance()
{
    static MsrpSessionStore s;
    return s;
}

MsrpSessionStore::MsrpSessionStore() : QObject(nullptr)
{
    qRegisterMetaType<MsrpSessionInfo>("MsrpSessionInfo");
}

void MsrpSessionStore::upsert(const MsrpSessionInfo &info)
{
    MsrpSessionInfo copy = info;
    copy.updatedAt = QDateTime::currentDateTimeUtc();
    if (!m_sessions.contains(copy.sessionKey))
        copy.createdAt = copy.updatedAt;
    else
        copy.createdAt = m_sessions.value(copy.sessionKey).createdAt;
    m_sessions.insert(copy.sessionKey, copy);
    emit sessionUpdated(copy);
}

MsrpSessionInfo MsrpSessionStore::get(const QString &sessionKey) const
{
    return m_sessions.value(sessionKey);
}

QList<MsrpSessionInfo> MsrpSessionStore::snapshot() const
{
    return m_sessions.values();
}

void MsrpSessionStore::remove(const QString &sessionKey)
{
    if (m_sessions.remove(sessionKey))
        emit sessionRemoved(sessionKey);
}

void MsrpSessionStore::clear()
{
    m_sessions.clear();
    emit cleared();
}
