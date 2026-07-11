#include "XcapDiagnosticsStore.h"

#include "sip/XcapClient.h"

XcapDiagnosticsStore &XcapDiagnosticsStore::instance()
{
    static XcapDiagnosticsStore s;
    return s;
}

XcapDiagnosticsStore::XcapDiagnosticsStore(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<XcapResult>("XcapResult");
    connect(&XcapClient::instance(), &XcapClient::operationCompleted,
            this, &XcapDiagnosticsStore::onOperationCompleted);
}

void XcapDiagnosticsStore::onOperationCompleted(const XcapResult &result)
{
    recordResult(result);
}

void XcapDiagnosticsStore::recordResult(const XcapResult &result)
{
    m_entries.append(result);
    while (m_entries.size() > kMaxRetainedEntries)
        m_entries.removeFirst();
    emit entryLogged(result);
}

const QList<XcapResult> &XcapDiagnosticsStore::entries() const
{
    return m_entries;
}

void XcapDiagnosticsStore::clear()
{
    m_entries.clear();
    emit cleared();
}
