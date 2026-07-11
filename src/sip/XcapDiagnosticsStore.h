#pragma once
#include <QList>
#include <QObject>

#include "sip/XcapModels.h"

// XCAP Diagnostics store (Task W099) — collects completed XcapClient
// operations for the "XCAP Diagnostics" UI page and the Interop JSON export
// ("xcapEvents"). XCAP is plain HTTP, not SIP, so this deliberately does not
// subscribe to SipTraceLogger and never feeds the SIP Ladder (requirement 11:
// "HTTP operations must not be drawn as SIP traffic").
class XcapDiagnosticsStore : public QObject
{
    Q_OBJECT
public:
    static XcapDiagnosticsStore &instance();

    const QList<XcapResult> &entries() const;
    void clear();

    // Appends a result, evicting the oldest once kMaxRetainedEntries is
    // exceeded, and emits entryLogged. Public (not just the private slot)
    // so unit tests can exercise the store's bounding/append behavior
    // without a live XcapClient network round-trip.
    void recordResult(const XcapResult &result);

    // Oldest entries are evicted once this many are retained.
    static constexpr int kMaxRetainedEntries = 500;

signals:
    void entryLogged(const XcapResult &result);
    void cleared();

private slots:
    void onOperationCompleted(const XcapResult &result);

private:
    explicit XcapDiagnosticsStore(QObject *parent = nullptr);

    QList<XcapResult> m_entries;
};
