#include "MainWindow.h"
#include "panels/NavRail.h"
#include "panels/SidebarPanel.h"
#include "panels/ContactsPanel.h"
#include "panels/CallPanel.h"
#include "panels/VideoPanel.h"
#include "panels/RttPanel.h"
#include "panels/DiagnosticsPanel.h"
#include "panels/MediaPanel.h"
#include "widgets/AppStatusBar.h"
#include <QDialog>
#include <QVBoxLayout>
#include "core/AppSettings.h"
#include "core/Logger.h"
#include "sip/SipManager.h"

#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QStackedWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("SIP Client - Audio / Video / RTT");
    setMinimumSize(1024, 768);
    resize(1440, 900);

    buildMenuBar();
    buildCentralWidget();
    buildStatusBar();
    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            m_statusBar, &AppStatusBar::setRegistrationStatus);
    restoreLayout();

    Logger::instance().info(LogCategory::App, "Main window initialized");
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveLayout();
    event->accept();
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------
void MainWindow::buildMenuBar()
{
    auto *mb = menuBar();

    // File
    auto *menuFile = mb->addMenu(tr("&File"));
    m_actNewCall = menuFile->addAction(tr("&New Call…"), this, [this] {
        // Switch to accounts page so profile is visible, then focus dial.
        onNavPageRequested(QStringLiteral("dialpad"));
    }, QKeySequence(Qt::CTRL | Qt::Key_N));
    menuFile->addSeparator();
    menuFile->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    // Contacts
    auto *menuContacts = mb->addMenu(tr("&Contacts"));
    m_actAddContact = menuContacts->addAction(tr("&Add Contact…"), this, [this] {
        onNavPageRequested(QStringLiteral("contacts"));
        if (m_contactsPanel)
            QMetaObject::invokeMethod(m_contactsPanel, "onAddContact", Qt::QueuedConnection);
    });
    auto *actImport = menuContacts->addAction(tr("&Import Contacts…"));
    actImport->setEnabled(false);
    actImport->setToolTip(tr("Not available in this release"));

    // Calls
    auto *menuCalls = mb->addMenu(tr("Ca&lls"));
    auto *actHistory = menuCalls->addAction(tr("&Call History"));
    actHistory->setEnabled(false);
    actHistory->setToolTip(tr("Not available in this release"));
    auto *actRedial = menuCalls->addAction(tr("&Redial"));
    actRedial->setEnabled(false);
    actRedial->setToolTip(tr("Not available in this release"));

    // View
    auto *menuView = mb->addMenu(tr("&View"));
    menuView->addAction(tr("&Audio / Media Settings"), this, &MainWindow::showSettingsDialog);
    menuView->addSeparator();
    menuView->addAction(tr("&Full Screen"), this,
                        &QMainWindow::showFullScreen, QKeySequence::FullScreen);

    // Messaging — placeholder, fully disabled
    auto *menuMsg = mb->addMenu(tr("&Messaging"));
    auto *actRtt  = menuMsg->addAction(tr("&New RTT Session…"));
    auto *actLmpe = menuMsg->addAction(tr("&New LMPE Message…"));
    actRtt->setEnabled(false);
    actRtt->setToolTip(tr("Not available in this release"));
    actLmpe->setEnabled(false);
    actLmpe->setToolTip(tr("Not available in this release"));

    // Tools
    auto *menuTools = mb->addMenu(tr("&Tools"));
    auto *actPrefs  = menuTools->addAction(tr("&Preferences…"));
    actPrefs->setEnabled(false);
    actPrefs->setToolTip(tr("Not available in this release"));
    menuTools->addAction(tr("&SIP Accounts…"), this, [this] {
        onNavPageRequested(QStringLiteral("accounts"));
    });
    menuTools->addSeparator();
    auto *actDebug = menuTools->addAction(tr("Export &Debug Bundle…"));
    actDebug->setEnabled(false);
    actDebug->setToolTip(tr("Not available in this release"));

    // Help
    auto *menuHelp = mb->addMenu(tr("&Help"));
    auto *actAbout = menuHelp->addAction(tr("&About SIP Client"));
    actAbout->setEnabled(false);
}

// ---------------------------------------------------------------------------
// Central widget layout
// ---------------------------------------------------------------------------
void MainWindow::buildCentralWidget()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Left navigation rail (fixed 64 px)
    m_navRail = new NavRail(central);
    m_navRail->setFixedWidth(64);
    connect(m_navRail, &NavRail::pageRequested,
            this, &MainWindow::onNavPageRequested);
    rootLayout->addWidget(m_navRail);

    // Vertical splitter: [main content area] over [diagnostics panel]
    m_vertSplitter = new QSplitter(Qt::Vertical, central);
    m_vertSplitter->setChildrenCollapsible(false);
    m_vertSplitter->setHandleWidth(4);
    rootLayout->addWidget(m_vertSplitter, 1);

    // --- Top part of vertical splitter ---
    m_horzSplitter = new QSplitter(Qt::Horizontal, m_vertSplitter);
    m_horzSplitter->setChildrenCollapsible(false);
    m_horzSplitter->setHandleWidth(4);
    m_vertSplitter->addWidget(m_horzSplitter);

    // Stacked sidebar: Accounts | Contacts  (switched by NavRail)
    m_sidebarStack = new QStackedWidget(m_horzSplitter);
    m_sidebarStack->setMinimumWidth(220);

    m_sidebar = new SidebarPanel(m_sidebarStack);
    m_sidebarStack->addWidget(m_sidebar);          // index 0 — accounts

    m_contactsPanel = new ContactsPanel(m_sidebarStack);
    m_sidebarStack->addWidget(m_contactsPanel);    // index 1 — contacts

    m_sidebarStack->setCurrentIndex(0);
    m_horzSplitter->addWidget(m_sidebarStack);

    // Center area: call panel + video
    auto *centerWidget = new QWidget(m_horzSplitter);
    centerWidget->setMinimumWidth(420);
    auto *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_callPanel = new CallPanel(centerWidget);
    centerLayout->addWidget(m_callPanel);

    m_videoPanel = new VideoPanel(centerWidget);
    centerLayout->addWidget(m_videoPanel, 1);

    m_horzSplitter->addWidget(centerWidget);

    // Right RTT panel — wired to SipManager::rttSession()
    m_rttPanel = new RttPanel(m_horzSplitter);
    m_rttPanel->setMinimumWidth(260);
    m_rttPanel->setRttSession(SipManager::instance().rttSession());
    m_horzSplitter->addWidget(m_rttPanel);

    // Initial splitter proportions: sidebar=260, center=flexible, right=320
    m_horzSplitter->setSizes({260, 800, 320});

    // --- Bottom part of vertical splitter: diagnostics ---
    m_diagnostics = new DiagnosticsPanel(m_vertSplitter);
    m_diagnostics->setMinimumHeight(160);
    m_vertSplitter->addWidget(m_diagnostics);

    // Vertical split: main area gets most space, diagnostics ~220px
    m_vertSplitter->setSizes({10000, 220});

    setCentralWidget(central);

    // Wire contacts → dial (m_callPanel now exists)
    connect(m_contactsPanel, &ContactsPanel::dialRequested,
            m_callPanel, &CallPanel::setDialTarget);
}

// ---------------------------------------------------------------------------
// Settings dialog (lazy, reused across invocations)
// ---------------------------------------------------------------------------
void MainWindow::showSettingsDialog()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new QDialog(this);
        m_settingsDialog->setWindowTitle(tr("Audio / Media Settings"));
        m_settingsDialog->setMinimumSize(400, 300);
        auto *dlgLayout = new QVBoxLayout(m_settingsDialog);
        dlgLayout->setContentsMargins(8, 8, 8, 8);
        dlgLayout->addWidget(new MediaPanel(m_settingsDialog));
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

// ---------------------------------------------------------------------------
// NavRail page handler
// ---------------------------------------------------------------------------
void MainWindow::onNavPageRequested(const QString &page)
{
    if (page == QLatin1String("accounts")) {
        m_sidebarStack->setCurrentIndex(0);
    } else if (page == QLatin1String("contacts")) {
        m_sidebarStack->setCurrentIndex(1);
    } else if (page == QLatin1String("dialpad")) {
        // Don't switch sidebar — just focus the dial input in the call panel.
        m_callPanel->focusDialInput();
    } else if (page == QLatin1String("settings")) {
        showSettingsDialog();
    }
    // "history" and "messages" buttons are disabled; no-op if somehow reached.
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------
void MainWindow::buildStatusBar()
{
    m_statusBar = new AppStatusBar(this);
    setStatusBar(m_statusBar);
}

// ---------------------------------------------------------------------------
// Layout persistence
// ---------------------------------------------------------------------------
void MainWindow::restoreLayout()
{
    auto geom = AppSettings::loadWindowGeometry();
    if (!geom.isEmpty())
        restoreGeometry(geom);

    auto hState = AppSettings::loadSplitterState("horizontal");
    if (!hState.isEmpty())
        m_horzSplitter->restoreState(hState);

    auto vState = AppSettings::loadSplitterState("vertical");
    if (!vState.isEmpty())
        m_vertSplitter->restoreState(vState);
}

void MainWindow::saveLayout()
{
    AppSettings::saveWindowGeometry(saveGeometry());
    AppSettings::saveSplitterState("horizontal", m_horzSplitter->saveState());
    AppSettings::saveSplitterState("vertical",   m_vertSplitter->saveState());
}

void MainWindow::updateSipBackendStatus()
{
    const auto &sip = SipManager::instance();
    m_statusBar->setSipBackend(sip.backendName(), sip.isInitialized());
    m_statusBar->setRegistrationStatus(sip.registrationState(),
                                       sip.registrationStatusText(),
                                       sip.registrationStatusCode());
}
