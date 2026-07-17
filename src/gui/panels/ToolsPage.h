#pragma once
#include <QWidget>

class QTabWidget;
class SipLadderPage;
class MessagingDiagnosticsPage;
class PresencePage;
class XcapPage;
class MsrpPage;
class DiagnosticsPanel;
class DiagnosticsCenterPanel;

// Tools: single nav destination hosting every technical/diagnostic/test page
// (SIP Ladder, Messaging diagnostics, Presence, XCAP, MSRP, Logs, Diagnostics
// center) as sub-tabs. Each sub-tab is still built lazily on first visit —
// switching to Tools does not eagerly construct all seven pages — mirroring
// MainWindow::ensurePage's m_pageBuilt guard pattern one level down.
class ToolsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ToolsPage(QWidget *parent = nullptr);

    // Programmatic navigation used by deep links (e.g. DiagnosticsCenterPanel's
    // "open SIP Ladder" / "open Logs" requests, and MainWindow routing old
    // top-level nav ids like "sipladder"/"logs" into this page's sub-tabs).
    void openSubTab(const QString &key);

    // Task W113 (Call Workspace) deep link: ensures the SIP Ladder sub-tab
    // exists, then filters it by callId. Callers should switch to Tools /
    // call openSubTab("sipladder") first so the tab is visible.
    void filterSipLadderByCallId(const QString &callId);

    static constexpr int kSubTabCount = 7;

private:
    void ensureSubTab(int index);
    int indexForKey(const QString &key) const;

    QTabWidget *m_tabs{nullptr};
    bool m_subTabBuilt[kSubTabCount]{};

    SipLadderPage           *m_ladderPage{nullptr};
    MessagingDiagnosticsPage *m_messagingPage{nullptr};
    PresencePage            *m_presencePage{nullptr};
    XcapPage                *m_xcapPage{nullptr};
    MsrpPage                *m_msrpPage{nullptr};
    DiagnosticsPanel        *m_logsPage{nullptr};
    DiagnosticsCenterPanel  *m_diagnosticsCenterPanel{nullptr};
};
