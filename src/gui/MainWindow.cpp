#include "MainWindow.h"
#include "panels/NavRail.h"
#include "panels/SidebarPanel.h"
#include "panels/CallPanel.h"
#include "panels/VideoPanel.h"
#include "panels/RttPanel.h"
#include "panels/DiagnosticsPanel.h"
#include "panels/MediaPanel.h"
#include "widgets/AppStatusBar.h"
#include "core/AppSettings.h"
#include "core/Logger.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("SIP Client - Audio / Video / RTT");
    setMinimumSize(1024, 768);
    resize(1440, 900);

    buildMenuBar();
    buildCentralWidget();
    buildStatusBar();
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
    menuFile->addAction(tr("&New Call..."));
    menuFile->addSeparator();
    menuFile->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    // View
    auto *menuView = mb->addMenu(tr("&View"));
    menuView->addAction(tr("Show &Diagnostics Panel"));
    menuView->addAction(tr("Show &RTT Panel"));
    menuView->addSeparator();
    menuView->addAction(tr("&Full Screen"), this, &QMainWindow::showFullScreen, QKeySequence::FullScreen);

    // Contacts
    auto *menuContacts = mb->addMenu(tr("&Contacts"));
    menuContacts->addAction(tr("&Add Contact..."));
    menuContacts->addAction(tr("&Import Contacts..."));

    // Calls
    auto *menuCalls = mb->addMenu(tr("Ca&lls"));
    menuCalls->addAction(tr("&Call History"));
    menuCalls->addAction(tr("&Redial"));

    // Messaging
    auto *menuMsg = mb->addMenu(tr("&Messaging"));
    menuMsg->addAction(tr("&New RTT Session..."));
    menuMsg->addAction(tr("&New LMPE Message..."));

    // Tools
    auto *menuTools = mb->addMenu(tr("&Tools"));
    menuTools->addAction(tr("&Preferences..."));
    menuTools->addAction(tr("&SIP Accounts..."));
    menuTools->addSeparator();
    menuTools->addAction(tr("Export &Debug Bundle..."));

    // Help
    auto *menuHelp = mb->addMenu(tr("&Help"));
    menuHelp->addAction(tr("&About SIP Client"));
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

    // Sidebar
    m_sidebar = new SidebarPanel(m_horzSplitter);
    m_sidebar->setMinimumWidth(220);
    m_horzSplitter->addWidget(m_sidebar);

    // Center area: call header + video + info tabs
    auto *centerWidget = new QWidget(m_horzSplitter);
    centerWidget->setMinimumWidth(420);
    auto *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_callPanel = new CallPanel(centerWidget);
    centerLayout->addWidget(m_callPanel);

    m_videoPanel = new VideoPanel(centerWidget);
    centerLayout->addWidget(m_videoPanel, 1);

    // Info tabs below video
    auto *infoTabs = new QTabWidget(centerWidget);
    infoTabs->setFixedHeight(200);
    infoTabs->addTab(new QWidget(), tr("Call Info"));
    m_mediaPanel = new MediaPanel(centerWidget);
    infoTabs->addTab(m_mediaPanel, tr("Media"));
    infoTabs->addTab(new QWidget(), tr("Statistics"));
    centerLayout->addWidget(infoTabs);

    m_horzSplitter->addWidget(centerWidget);

    // Right RTT/LMPE panel
    m_rttPanel = new RttPanel(m_horzSplitter);
    m_rttPanel->setMinimumWidth(300);
    m_horzSplitter->addWidget(m_rttPanel);

    // Set initial splitter sizes: sidebar=260, center=flexible, right=380
    m_horzSplitter->setSizes({260, 800, 380});

    // --- Bottom part of vertical splitter: diagnostics ---
    m_diagnostics = new DiagnosticsPanel(m_vertSplitter);
    m_diagnostics->setMinimumHeight(160);
    m_vertSplitter->addWidget(m_diagnostics);

    // Vertical split: give main area most space, diagnostics ~240px
    m_vertSplitter->setSizes({10000, 240});

    setCentralWidget(central);
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
