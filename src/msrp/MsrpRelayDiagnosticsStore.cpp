#include "MsrpRelayDiagnosticsStore.h"

MsrpRelayDiagnosticsStore &MsrpRelayDiagnosticsStore::instance()
{
    static MsrpRelayDiagnosticsStore s;
    return s;
}

MsrpRelayDiagnosticsStore::MsrpRelayDiagnosticsStore() : QObject(nullptr)
{
    qRegisterMetaType<MsrpRelayDiagnosticsEvent>("MsrpRelayDiagnosticsEvent");
}

void MsrpRelayDiagnosticsStore::record(const MsrpRelayDiagnosticsEvent &event)
{
    m_entries.append(event);
    while (m_entries.size() > kMaxRetainedEntries)
        m_entries.removeFirst();
    emit entryLogged(event);
}

const QList<MsrpRelayDiagnosticsEvent> &MsrpRelayDiagnosticsStore::entries() const
{
    return m_entries;
}

void MsrpRelayDiagnosticsStore::clear()
{
    m_entries.clear();
    emit cleared();
}
