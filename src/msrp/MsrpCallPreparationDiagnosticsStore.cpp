#include "MsrpCallPreparationDiagnosticsStore.h"

MsrpCallPreparationDiagnosticsStore &MsrpCallPreparationDiagnosticsStore::instance()
{
    static MsrpCallPreparationDiagnosticsStore s;
    return s;
}

MsrpCallPreparationDiagnosticsStore::MsrpCallPreparationDiagnosticsStore() : QObject(nullptr)
{
    qRegisterMetaType<MsrpCallPreparationDiagnosticsEvent>("MsrpCallPreparationDiagnosticsEvent");
}

void MsrpCallPreparationDiagnosticsStore::record(const MsrpCallPreparationDiagnosticsEvent &event)
{
    m_entries.append(event);
    while (m_entries.size() > kMaxRetainedEntries)
        m_entries.removeFirst();
    emit entryLogged(event);
}

const QList<MsrpCallPreparationDiagnosticsEvent> &MsrpCallPreparationDiagnosticsStore::entries() const
{
    return m_entries;
}

void MsrpCallPreparationDiagnosticsStore::clear()
{
    m_entries.clear();
    emit cleared();
}
