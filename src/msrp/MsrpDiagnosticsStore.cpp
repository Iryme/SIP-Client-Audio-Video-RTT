#include "MsrpDiagnosticsStore.h"

MsrpDiagnosticsStore &MsrpDiagnosticsStore::instance()
{
    static MsrpDiagnosticsStore s;
    return s;
}

MsrpDiagnosticsStore::MsrpDiagnosticsStore() : QObject(nullptr)
{
    qRegisterMetaType<MsrpDiagnosticsEvent>("MsrpDiagnosticsEvent");
}

void MsrpDiagnosticsStore::record(const MsrpDiagnosticsEvent &event)
{
    m_entries.append(event);
    while (m_entries.size() > kMaxRetainedEntries)
        m_entries.removeFirst();
    emit entryLogged(event);
}

const QList<MsrpDiagnosticsEvent> &MsrpDiagnosticsStore::entries() const
{
    return m_entries;
}

void MsrpDiagnosticsStore::clear()
{
    m_entries.clear();
    emit cleared();
}
