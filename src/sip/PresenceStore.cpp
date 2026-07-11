#include "PresenceStore.h"

#include <QMutexLocker>

#include "core/AppSettings.h"

PresenceStore &PresenceStore::instance()
{
    static PresenceStore s;
    return s;
}

PresenceStore::PresenceStore(int maxRetainedEvents)
    : QObject(nullptr)
    , m_maxRetainedEvents(maxRetainedEvents > 0 ? maxRetainedEvents : 500)
{
    qRegisterMetaType<PresenceInfo>("PresenceInfo");

    const int configured = AppSettings::presenceMaxRetainedEvents();
    if (configured > 0)
        m_maxRetainedEvents = configured;
}

void PresenceStore::upsert(const PresenceInfo &info)
{
    if (info.entityUri.trimmed().isEmpty())
        return;

    {
        QMutexLocker locker(&m_mutex);
        m_current.insert(info.entityUri, info);
        m_history.append(info);
        while (m_history.size() > m_maxRetainedEvents)
            m_history.removeFirst();
    }
    emit presenceUpdated(info);
}

PresenceInfo PresenceStore::current(const QString &entityUri) const
{
    QMutexLocker locker(&m_mutex);
    return m_current.value(entityUri);
}

QList<PresenceInfo> PresenceStore::snapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_current.values();
}

QList<PresenceInfo> PresenceStore::history() const
{
    QMutexLocker locker(&m_mutex);
    return m_history;
}

void PresenceStore::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_current.clear();
        m_history.clear();
    }
    emit cleared();
}

int PresenceStore::maxRetainedEvents() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxRetainedEvents;
}

void PresenceStore::setMaxRetainedEvents(int max)
{
    if (max <= 0)
        return;
    QMutexLocker locker(&m_mutex);
    m_maxRetainedEvents = max;
    while (m_history.size() > m_maxRetainedEvents)
        m_history.removeFirst();
}
