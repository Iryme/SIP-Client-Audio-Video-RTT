#include "ToolsPage.h"

#include "SipLadderPage.h"
#include "MessagingDiagnosticsPage.h"
#include "PresencePage.h"
#include "XcapPage.h"
#include "MsrpPage.h"
#include "DiagnosticsPanel.h"
#include "DiagnosticsCenterPanel.h"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QWidget *makeTabPlaceholder(const QString &name, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *lay = new QVBoxLayout(w);
    auto *lbl = new QLabel(QStringLiteral("Loading %1…").arg(name), w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color: #555566; font-size: 14px;");
    lay->addWidget(lbl);
    return w;
}

// Sub-tab order/index mapping, shared between construction and key lookup.
constexpr int kIndexSipLadder = 0;
constexpr int kIndexMessaging = 1;
constexpr int kIndexPresence = 2;
constexpr int kIndexXcap = 3;
constexpr int kIndexMsrp = 4;
constexpr int kIndexLogs = 5;
constexpr int kIndexDiagnostics = 6;

} // namespace

ToolsPage::ToolsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ToolsPage"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("toolsTabs"));
    root->addWidget(m_tabs);

    static const char *const kTabNames[ToolsPage::kSubTabCount] = {
        "SIP Ladder", "Messaging", "Presence", "XCAP", "MSRP", "Logs", "Diagnostics"
    };
    for (int i = 0; i < kSubTabCount; ++i) {
        m_tabs->addTab(makeTabPlaceholder(tr(kTabNames[i]), m_tabs), tr(kTabNames[i]));
        m_subTabBuilt[i] = false;
    }

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        ensureSubTab(index);
    });

    // Build the first sub-tab (SIP Ladder) immediately since it's shown by
    // default when Tools is first opened — mirrors MainWindow's Dashboard
    // eager-build-on-default-page behavior.
    ensureSubTab(kIndexSipLadder);
}

void ToolsPage::ensureSubTab(int index)
{
    if (index < 0 || index >= kSubTabCount || m_subTabBuilt[index])
        return;

    m_subTabBuilt[index] = true;

    QWidget *real = nullptr;
    switch (index) {
    case kIndexSipLadder:
        m_ladderPage = new SipLadderPage(m_tabs);
        real = m_ladderPage;
        break;
    case kIndexMessaging:
        m_messagingPage = new MessagingDiagnosticsPage(m_tabs);
        real = m_messagingPage;
        break;
    case kIndexPresence:
        m_presencePage = new PresencePage(m_tabs);
        real = m_presencePage;
        break;
    case kIndexXcap:
        m_xcapPage = new XcapPage(m_tabs);
        real = m_xcapPage;
        break;
    case kIndexMsrp:
        m_msrpPage = new MsrpPage(m_tabs);
        real = m_msrpPage;
        break;
    case kIndexLogs:
        m_logsPage = new DiagnosticsPanel(m_tabs);
        real = m_logsPage;
        break;
    case kIndexDiagnostics:
        m_diagnosticsCenterPanel = new DiagnosticsCenterPanel(m_tabs);
        connect(m_diagnosticsCenterPanel, &DiagnosticsCenterPanel::openSipLadderRequested,
                this, [this]() { openSubTab(QStringLiteral("sipladder")); });
        connect(m_diagnosticsCenterPanel, &DiagnosticsCenterPanel::openLogsRequested,
                this, [this]() { openSubTab(QStringLiteral("logs")); });
        real = m_diagnosticsCenterPanel;
        break;
    default:
        return;
    }

    const QString label = m_tabs->tabText(index);
    QWidget *old = m_tabs->widget(index);
    m_tabs->removeTab(index);
    m_tabs->insertTab(index, real, label);
    old->deleteLater();
}

int ToolsPage::indexForKey(const QString &key) const
{
    if (key == QLatin1String("sipladder")) return kIndexSipLadder;
    if (key == QLatin1String("messaging")) return kIndexMessaging;
    if (key == QLatin1String("presence")) return kIndexPresence;
    if (key == QLatin1String("xcap")) return kIndexXcap;
    if (key == QLatin1String("msrp")) return kIndexMsrp;
    if (key == QLatin1String("logs")) return kIndexLogs;
    if (key == QLatin1String("diagnostics")) return kIndexDiagnostics;
    return -1;
}

void ToolsPage::openSubTab(const QString &key)
{
    const int index = indexForKey(key);
    if (index < 0)
        return;
    ensureSubTab(index);
    m_tabs->setCurrentIndex(index);
}
