#pragma once
#include <QList>
#include <QObject>

#include "msrp/MsrpRelayDiagnosticsEvent.h"

// Relay diagnostics store (Task W107) — mirrors MsrpDiagnosticsStore.
class MsrpRelayDiagnosticsStore : public QObject
{
    Q_OBJECT
public:
    static MsrpRelayDiagnosticsStore &instance();

    void record(const MsrpRelayDiagnosticsEvent &event);
    const QList<MsrpRelayDiagnosticsEvent> &entries() const;
    void clear();

    static constexpr int kMaxRetainedEntries = 1000;

signals:
    void entryLogged(const MsrpRelayDiagnosticsEvent &event);
    void cleared();

private:
    MsrpRelayDiagnosticsStore();

    QList<MsrpRelayDiagnosticsEvent> m_entries;
};
