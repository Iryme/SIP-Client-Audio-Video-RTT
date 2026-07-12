#pragma once
#include <QList>
#include <QObject>

#include "msrp/MsrpDiagnosticsEvent.h"

// MSRP diagnostics store (Task W100, section Q) — collects
// MsrpDiagnosticsEvent rows for the MSRP page's frame log and the Interop
// JSON export's msrpEvents array. Thread-confined to the Qt thread; every
// producer (MsrpSession/MsrpTransport) already runs on the Qt event loop
// (see docs/msrp-transport.md), so no cross-thread marshaling is needed.
class MsrpDiagnosticsStore : public QObject
{
    Q_OBJECT
public:
    static MsrpDiagnosticsStore &instance();

    void record(const MsrpDiagnosticsEvent &event);
    const QList<MsrpDiagnosticsEvent> &entries() const;
    void clear();

    static constexpr int kMaxRetainedEntries = 1000;

signals:
    void entryLogged(const MsrpDiagnosticsEvent &event);
    void cleared();

private:
    MsrpDiagnosticsStore();

    QList<MsrpDiagnosticsEvent> m_entries;
};
