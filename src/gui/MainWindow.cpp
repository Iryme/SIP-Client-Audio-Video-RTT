#include "MainWindow.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "gui/panels/CallPanel.h"
#include "gui/panels/ContactsPanel.h"
#include "gui/panels/DiagnosticsPanel.h"
#include "gui/panels/MediaPanel.h"
#include "gui/panels/NavRail.h"
#include "gui/panels/SettingsPanel.h"
#include "gui/panels/SidebarPanel.h"
#include "gui/panels/SipLadderPage.h"
#include "gui/panels/VideoPanel.h"
#include "gui/panels/RttPanel.h"
#include "gui/widgets/AppStatusBar.h"
#include "sip/SipManager.h"

#include <QAction>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {
static QFrame *makeCard(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setObjectName("DashboardCard");
    return frame;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("SIP Client - Audio / Video / RTT");
    setMinimumSize(1180, 820);
    resize(1500, 960);

    buildMenuBar();
    buildCentralWidget();
    buildStatusBar();

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            m_statusBar, &AppStatusBar::setRegistrationStatus);
    restoreLayout();

    if (m_navRail)
        m_navRail->setPageActive(QStringLiteral("dashboard"));
    if (m_pageStack)
        m_pageStack->setCurrentIndex(0);

    Logger::instance().info(LogCategory::App, "Main window initialized");
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveLayout();
    event->accept();
}

void MainWindow::buildMenuBar()
{
    auto *mb = menuBar();

    auto *menuFile = mb->addMenu(tr("&File"));
    m_actNewCall = menuFile->addAction(tr("&New Call..."), this, [this] {
        onNavPageRequested(QStringLiteral("clients"));
        if (m_callPanel)
            m_callPanel->focusDialInput();
    }, QKeySequence(Qt::CTRL | Qt::Key_N));
    menuFile->addSeparator();
    menuFile->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    auto *menuContacts = mb->addMenu(tr("&Contacts"));
    m_actAddContact = menuContacts->addAction(tr("&Add Contact..."), this, [this] {
        onNavPageRequested(QStringLiteral("clients"));
    });
    auto *actImport = menuContacts->addAction(tr("&Import Contacts..."));
    actImport->setEnabled(false);
    actImport->setToolTip(tr("Not available in this release"));

    auto *menuCalls = mb->addMenu(tr("Ca&lls"));
    auto *actHistory = menuCalls->addAction(tr("&Call History"));
    actHistory->setEnabled(false);
    auto *actRedial = menuCalls->addAction(tr("&Redial"));
    actRedial->setEnabled(false);

    auto *menuView = mb->addMenu(tr("&View"));
    menuView->addAction(tr("&Dashboard"), this, [this] { onNavPageRequested(QStringLiteral("dashboard")); });
    menuView->addAction(tr("&Settings"), this, [this] { onNavPageRequested(QStringLiteral("settings")); });
    menuView->addSeparator();
    menuView->addAction(tr("&Full Screen"), this,
                        &QMainWindow::showFullScreen, QKeySequence::FullScreen);

    auto *menuMsg = mb->addMenu(tr("&Messaging"));
    auto *actRtt  = menuMsg->addAction(tr("&New RTT Session..."));
    auto *actLmpe = menuMsg->addAction(tr("&New LMPE Message..."));
    actRtt->setEnabled(false);
    actLmpe->setEnabled(false);

    auto *menuTools = mb->addMenu(tr("&Tools"));
    auto *actPrefs  = menuTools->addAction(tr("&Preferences..."));
    actPrefs->setEnabled(false);
    menuTools->addAction(tr("&SIP Accounts..."), this, [this] {
        onNavPageRequested(QStringLiteral("clients"));
    });
    menuTools->addSeparator();
    auto *actDebug = menuTools->addAction(tr("Export &Debug Bundle..."));
    actDebug->setEnabled(false);

    auto *menuHelp = mb->addMenu(tr("&Help"));
    auto *actAbout = menuHelp->addAction(tr("&About SIP Client"));
    actAbout->setEnabled(false);
}

QWidget *MainWindow::buildDashboardPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *title = new QLabel(tr("Dashboard"), page);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Live overview of the SIP backend, current registration, and recent events."),
        page);
    subtitle->setStyleSheet("color: #b7c4d6;");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto *cardRow = new QHBoxLayout();
    cardRow->setSpacing(8);

    auto addCard = [&](const QString &label, const QString &value) -> QLabel* {
        auto *card = makeCard(page);
        auto *lay = new QVBoxLayout(card);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->setSpacing(2);
        auto *lbl = new QLabel(label, card);
        lbl->setStyleSheet("color: #8899aa; font-size: 11px;");
        auto *val = new QLabel(value, card);
        val->setWordWrap(true);
        val->setStyleSheet("font-size: 16px; font-weight: 600;");
        lay->addWidget(lbl);
        lay->addWidget(val);
        cardRow->addWidget(card, 1);
        return val;
    };

    QLabel *backendValue = addCard(tr("Backend / WS"), tr("Initializing..."));
    QLabel *registrationValue = addCard(tr("Registered users"), tr("0"));
    QLabel *callsValue = addCard(tr("Active calls"), tr("0"));
    QLabel *eventValue = addCard(tr("Last SIP event"), tr("Waiting for activity"));

    root->addLayout(cardRow);

    auto *logCard = makeCard(page);
    auto *logLayout = new QVBoxLayout(logCard);
    logLayout->setContentsMargins(10, 10, 10, 10);
    logLayout->setSpacing(6);
    auto *logTitle = new QLabel(tr("Recent SIP / backend events"), logCard);
    logTitle->setStyleSheet("font-weight: 600;");
    logLayout->addWidget(logTitle);

    auto *logTable = new QTableWidget(0, 3, logCard);
    logTable->setHorizontalHeaderLabels({tr("Time"), tr("Level"), tr("Message")});
    logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    logTable->setSelectionBehavior(QTableWidget::SelectRows);
    logTable->setEditTriggers(QTableWidget::NoEditTriggers);
    logTable->setWordWrap(false);
    logTable->verticalHeader()->setVisible(false);
    logLayout->addWidget(logTable, 1);
    root->addWidget(logCard, 1);

    auto updateStatus = [backendValue, registrationValue, callsValue, eventValue]() {
        const auto &sip = SipManager::instance();
        backendValue->setText(QStringLiteral("%1 (%2)")
            .arg(sip.backendName(),
                 sip.isInitialized() ? QStringLiteral("ready") : QStringLiteral("inactive")));
        registrationValue->setText(!sip.registeredProfileId().isEmpty()
            && sip.registrationState() == RegistrationState::Registered
                ? QStringLiteral("1")
                : QStringLiteral("0"));
        callsValue->setText(sip.callState() == CallState::Idle ? QStringLiteral("0") : QStringLiteral("1"));
    };
    updateStatus();

    connect(&SipManager::instance(), &SipManager::initialized, page, updateStatus);
    connect(&SipManager::instance(), &SipManager::shutdownComplete, page, updateStatus);
    connect(&SipManager::instance(), &SipManager::callStateChanged, page, [updateStatus]() { updateStatus(); });
    connect(&SipManager::instance(), &SipManager::registrationStateChanged, page, [updateStatus]() { updateStatus(); });

    connect(&Logger::instance(), &Logger::entryAdded, page,
            [logTable, eventValue](const LogEntry &entry) {
        if (logTable->rowCount() >= 8)
            logTable->removeRow(logTable->rowCount() - 1);
        logTable->insertRow(0);
        logTable->setItem(0, 0, new QTableWidgetItem(entry.timestamp.toString("hh:mm:ss")));
        logTable->setItem(0, 1, new QTableWidgetItem(Logger::levelName(entry.level)));
        logTable->setItem(0, 2, new QTableWidgetItem(entry.message));
        if (entry.category == LogCategory::Sip)
            eventValue->setText(entry.message);
    });

    return page;
}

QWidget *MainWindow::buildClientsPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *header = new QLabel(tr("Clients / Call Control"), page);
    header->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(header);

    auto *desc = new QLabel(
        tr("Three-zone workspace for profiles, live call control, and contact-target actions."),
        page);
    desc->setStyleSheet("color: #b7c4d6;");
    desc->setWordWrap(true);
    root->addWidget(desc);

    auto *split = new QSplitter(Qt::Horizontal, page);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);

    auto *leftWrap = new QWidget(split);
    auto *leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_sidebar = new SidebarPanel(leftWrap);
    leftLayout->addWidget(m_sidebar);
    split->addWidget(leftWrap);

    auto *centerWrap = new QWidget(split);
    auto *centerLayout = new QVBoxLayout(centerWrap);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);
    m_callPanel = new CallPanel(centerWrap);
    centerLayout->addWidget(m_callPanel, 2);
    m_rttPanel = new RttPanel(centerWrap);
    centerLayout->addWidget(m_rttPanel, 1);
    split->addWidget(centerWrap);

    auto *rightWrap = new QWidget(split);
    auto *rightLayout = new QVBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_contactsPanel = new ContactsPanel(rightWrap);
    rightLayout->addWidget(m_contactsPanel);
    split->addWidget(rightWrap);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 1);
    split->setSizes(QList<int>{320, 620, 320});

    root->addWidget(split, 1);

    connect(m_contactsPanel, &ContactsPanel::dialRequested,
            m_callPanel, &CallPanel::placeCall);
    connect(m_contactsPanel, &ContactsPanel::dialRequested,
            m_callPanel, &CallPanel::focusDialInput);

    m_rttPanel->setRttSession(SipManager::instance().rttSession());
    return page;
}

QWidget *MainWindow::buildLogsPage()
{
    m_diagnostics = new DiagnosticsPanel(this);
    return m_diagnostics;
}

QWidget *MainWindow::buildMediaPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *title = new QLabel(tr("Media / Devices"), page);
    title->setStyleSheet("font-size: 18px; font-weight: 600;");
    root->addWidget(title);

    auto *desc = new QLabel(
        tr("Audio, camera and preview settings are kept here. The preview panel remains stable across resize."),
        page);
    desc->setStyleSheet("color: #b7c4d6;");
    desc->setWordWrap(true);
    root->addWidget(desc);

    auto *split = new QSplitter(Qt::Vertical, page);
    split->setChildrenCollapsible(false);
    m_videoPanel = new VideoPanel(split);
    m_videoPanel->setMinimumHeight(360);
    split->addWidget(m_videoPanel);

    auto *mediaPanel = new MediaPanel(split);
    split->addWidget(mediaPanel);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);

    root->addWidget(split, 1);
    return page;
}

void MainWindow::buildCentralWidget()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_navRail = new NavRail(central);
    m_navRail->setFixedWidth(72);
    connect(m_navRail, &NavRail::pageRequested,
            this, &MainWindow::onNavPageRequested);
    rootLayout->addWidget(m_navRail);

    m_pageStack = new QStackedWidget(central);
    rootLayout->addWidget(m_pageStack, 1);

    m_pageStack->addWidget(buildDashboardPage());
    m_pageStack->addWidget(buildClientsPage());
    m_ladderPage = new SipLadderPage(m_pageStack);
    m_pageStack->addWidget(m_ladderPage);
    m_pageStack->addWidget(buildLogsPage());
    m_pageStack->addWidget(buildMediaPage());
    m_settingsPanel = new SettingsPanel(m_pageStack);
    m_pageStack->addWidget(m_settingsPanel);

    setCentralWidget(central);

}

void MainWindow::showSettingsDialog()
{
    onNavPageRequested(QStringLiteral("settings"));
}

void MainWindow::onNavPageRequested(const QString &page)
{
    QString activePage = page;
    if (page == QLatin1String("dashboard")) {
        m_pageStack->setCurrentIndex(0);
    } else if (page == QLatin1String("clients") || page == QLatin1String("accounts")
               || page == QLatin1String("contacts") || page == QLatin1String("dialpad")) {
        m_pageStack->setCurrentIndex(1);
        activePage = QStringLiteral("clients");
        if (page == QLatin1String("dialpad") && m_callPanel)
            m_callPanel->focusDialInput();
    } else if (page == QLatin1String("sipladder")) {
        m_pageStack->setCurrentIndex(2);
    } else if (page == QLatin1String("logs")) {
        m_pageStack->setCurrentIndex(3);
    } else if (page == QLatin1String("media")) {
        m_pageStack->setCurrentIndex(4);
    } else if (page == QLatin1String("settings")) {
        m_pageStack->setCurrentIndex(5);
    }

    if (m_navRail)
        m_navRail->setPageActive(activePage);
}

void MainWindow::buildStatusBar()
{
    m_statusBar = new AppStatusBar(this);
    setStatusBar(m_statusBar);
}

void MainWindow::restoreLayout()
{
    auto geom = AppSettings::loadWindowGeometry();
    if (!geom.isEmpty())
        restoreGeometry(geom);
}

void MainWindow::saveLayout()
{
    AppSettings::saveWindowGeometry(saveGeometry());
}

void MainWindow::updateSipBackendStatus()
{
    const auto &sip = SipManager::instance();
    m_statusBar->setSipBackend(sip.backendName(), sip.isInitialized());
    m_statusBar->setRegistrationStatus(sip.registrationState(),
                                       sip.registrationStatusText(),
                                       sip.registrationStatusCode());
}
