#pragma once
#include <QList>
#include <QObject>

#include "msrp/MsrpCallPreparationDiagnosticsEvent.h"

// Call-preparation diagnostics store (Task W108) — mirrors
// MsrpRelayDiagnosticsStore.
class MsrpCallPreparationDiagnosticsStore : public QObject
{
    Q_OBJECT
public:
    static MsrpCallPreparationDiagnosticsStore &instance();

    void record(const MsrpCallPreparationDiagnosticsEvent &event);
    const QList<MsrpCallPreparationDiagnosticsEvent> &entries() const;
    void clear();

    static constexpr int kMaxRetainedEntries = 1000;

signals:
    void entryLogged(const MsrpCallPreparationDiagnosticsEvent &event);
    void cleared();

private:
    MsrpCallPreparationDiagnosticsStore();

    QList<MsrpCallPreparationDiagnosticsEvent> m_entries;
};
