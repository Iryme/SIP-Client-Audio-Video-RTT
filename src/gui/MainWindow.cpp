#include "MainWindow.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "core/PerfScope.h"
#include "gui/dashboard/DashboardPage.h"
#include "gui/panels/CallPanel.h"
#include "gui/panels/DiagnosticsPanel.h"
#include "gui/panels/MediaPanel.h"
#include "gui/panels/NavRail.h"
#include "gui/panels/SettingsPanel.h"
#include "gui/panels/SipLadderPage.h"
#include "gui/panels/VideoPanel.h"
#include "gui/panels/RttPanel.h"
#include "gui/widgets/AppStatusBar.h"
#include "sip/SipProfileManager.h"
#include "sip/SipManager.h"

#include <QAction>
#include <QFrame>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QList>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>

namespace {

static QWidget *makePlaceholder(const QString &name, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *lay = new QVBoxLayout(w);
    auto *lbl = new QLabel(QStringLiteral("Loading %1…").arg(name), w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("color: #555566; font-size: 14px;");
    lay->addWidget(lbl);
    return w;
}

static QFrame *makeCard(QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setObjectName("DashboardCard");
    return frame;
}

static QString base64Encode(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64());
}

static QByteArray base64Decode(const QJsonValue &value)
{
    if (!value.isString())
        return {};
    return QByteArray::fromBase64(value.toString().toLatin1());
}

static QJsonObject exportSettingsGroup(QSettings &settings, const QString &group)
{
    QJsonObject obj;
    settings.beginGroup(group);
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys)
        obj.insert(key, base64Encode(settings.value(key).toByteArray()));
    settings.endGroup();
    return obj;
}

static void importSettingsGroup(QSettings &settings, const QString &group, const QJsonObject &obj)
{
    if (obj.isEmpty())
        return;
    settings.beginGroup(group);
    for (auto it = obj.begin(); it != obj.end(); ++it)
        settings.setValue(it.key(), base64Decode(it.value()));
    settings.endGroup();
}

static QJsonObject exportAppSettings()
{
    auto &settings = AppSettings::settings();
    QJsonObject root;

    QJsonObject ui;
    ui.insert(QStringLiteral("geometry"),
              base64Encode(settings.value(QStringLiteral("ui/geometry")).toByteArray()));
    ui.insert(QStringLiteral("state"),
              base64Encode(settings.value(QStringLiteral("ui/state")).toByteArray()));
    ui.insert(QStringLiteral("splitter"),
              exportSettingsGroup(settings, QStringLiteral("ui/splitter")));
    root.insert(QStringLiteral("ui"), ui);

    QJsonObject connection;
    connection.insert(QStringLiteral("serverIp"),
                      settings.value(QStringLiteral("connection/serverIp")).toString());
    connection.insert(QStringLiteral("sipDomain"),
                      settings.value(QStringLiteral("connection/sipDomain")).toString());
    connection.insert(QStringLiteral("sipPort"),
                      settings.value(QStringLiteral("connection/sipPort")).toString());
    connection.insert(QStringLiteral("wsUrl"),
                      settings.value(QStringLiteral("connection/wsUrl")).toString());
    connection.insert(QStringLiteral("debugSip"),
                      settings.value(QStringLiteral("connection/debugSip")).toBool());
    connection.insert(QStringLiteral("rawSip"),
                      settings.value(QStringLiteral("connection/rawSip")).toBool());
    connection.insert(QStringLiteral("persistMedia"),
                      settings.value(QStringLiteral("connection/persistMedia"), true).toBool());
    root.insert(QStringLiteral("connection"), connection);

    QJsonObject media;
    media.insert(QStringLiteral("microphone"),
                 settings.value(QStringLiteral("media/device/microphone")).toString());
    media.insert(QStringLiteral("speaker"),
                 settings.value(QStringLiteral("media/device/speaker")).toString());
    media.insert(QStringLiteral("camera"),
                 settings.value(QStringLiteral("media/device/camera")).toString());
    root.insert(QStringLiteral("media"), media);

    QJsonObject emergency;
    emergency.insert(QStringLiteral("testMode"),
                     settings.value(QStringLiteral("emergency/testMode"), false).toBool());
    emergency.insert(QStringLiteral("target"),
                     settings.value(QStringLiteral("emergency/target"),
                                    QStringLiteral("sip:psap@10.2.0.180")).toString());
    root.insert(QStringLiteral("emergency"), emergency);

    QJsonObject logLevels;
    settings.beginGroup(QStringLiteral("log/level"));
    for (const QString &key : settings.childKeys())
        logLevels.insert(key, settings.value(key).toBool());
    settings.endGroup();
    root.insert(QStringLiteral("logLevels"), logLevels);

    return root;
}

static void importAppSettings(const QJsonObject &root)
{
    auto &settings = AppSettings::settings();

    const QJsonObject ui = root.value(QStringLiteral("ui")).toObject();
    if (!ui.isEmpty()) {
        settings.setValue(QStringLiteral("ui/geometry"), base64Decode(ui.value(QStringLiteral("geometry"))));
        settings.setValue(QStringLiteral("ui/state"), base64Decode(ui.value(QStringLiteral("state"))));
        importSettingsGroup(settings, QStringLiteral("ui/splitter"),
                            ui.value(QStringLiteral("splitter")).toObject());
    }

    const QJsonObject connection = root.value(QStringLiteral("connection")).toObject();
    if (!connection.isEmpty()) {
        if (connection.contains(QStringLiteral("serverIp")))
            settings.setValue(QStringLiteral("connection/serverIp"),
                              connection.value(QStringLiteral("serverIp")).toString());
        if (connection.contains(QStringLiteral("sipDomain")))
            settings.setValue(QStringLiteral("connection/sipDomain"),
                              connection.value(QStringLiteral("sipDomain")).toString());
        if (connection.contains(QStringLiteral("sipPort")))
            settings.setValue(QStringLiteral("connection/sipPort"),
                              connection.value(QStringLiteral("sipPort")).toString());
        if (connection.contains(QStringLiteral("wsUrl")))
            settings.setValue(QStringLiteral("connection/wsUrl"),
                              connection.value(QStringLiteral("wsUrl")).toString());
        if (connection.contains(QStringLiteral("debugSip")))
            settings.setValue(QStringLiteral("connection/debugSip"),
                              connection.value(QStringLiteral("debugSip")).toBool());
        if (connection.contains(QStringLiteral("rawSip")))
            settings.setValue(QStringLiteral("connection/rawSip"),
                              connection.value(QStringLiteral("rawSip")).toBool());
        if (connection.contains(QStringLiteral("persistMedia")))
            settings.setValue(QStringLiteral("connection/persistMedia"),
                              connection.value(QStringLiteral("persistMedia")).toBool());
    }

    const QJsonObject media = root.value(QStringLiteral("media")).toObject();
    if (!media.isEmpty()) {
        if (media.contains(QStringLiteral("microphone")))
            settings.setValue(QStringLiteral("media/device/microphone"),
                              media.value(QStringLiteral("microphone")).toString());
        if (media.contains(QStringLiteral("speaker")))
            settings.setValue(QStringLiteral("media/device/speaker"),
                              media.value(QStringLiteral("speaker")).toString());
        if (media.contains(QStringLiteral("camera")))
            settings.setValue(QStringLiteral("media/device/camera"),
                              media.value(QStringLiteral("camera")).toString());
    }

    const QJsonObject emergency = root.value(QStringLiteral("emergency")).toObject();
    if (!emergency.isEmpty()) {
        if (emergency.contains(QStringLiteral("testMode")))
            settings.setValue(QStringLiteral("emergency/testMode"),
                              emergency.value(QStringLiteral("testMode")).toBool());
        if (emergency.contains(QStringLiteral("target")))
            settings.setValue(QStringLiteral("emergency/target"),
                              emergency.value(QStringLiteral("target")).toString());
    }

    const QJsonObject logLevels = root.value(QStringLiteral("logLevels")).toObject();
    if (!logLevels.isEmpty()) {
        settings.beginGroup(QStringLiteral("log/level"));
        for (auto it = logLevels.begin(); it != logLevels.end(); ++it)
            settings.setValue(it.key(), it.value().toBool());
        settings.endGroup();
    }

    settings.sync();
}

static QJsonObject profileToJson(const SipProfile &profile, bool passwordExported = false)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("profileId"), profile.profileId);
    obj.insert(QStringLiteral("displayName"), profile.displayName);
    obj.insert(QStringLiteral("sipUsername"), profile.sipUsername);
    obj.insert(QStringLiteral("sipDomain"), profile.sipDomain);
    obj.insert(QStringLiteral("sipUri"), profile.sipUri);
    obj.insert(QStringLiteral("registrar"), profile.registrar);
    obj.insert(QStringLiteral("proxy"), profile.proxy);
    obj.insert(QStringLiteral("outboundProxy"), profile.outboundProxy);
    obj.insert(QStringLiteral("transport"), SipProfileManager::transportToString(profile.transport));
    obj.insert(QStringLiteral("authUsername"), profile.authUsername);
    obj.insert(QStringLiteral("emergencyServiceUri"), profile.emergencyServiceUri);
    obj.insert(QStringLiteral("enableRtt"), profile.enableRtt);
    obj.insert(QStringLiteral("enableLmpe"), profile.enableLmpe);
    obj.insert(QStringLiteral("enableEtsiCompatibility"), profile.enableEtsiCompatibility);
    obj.insert(QStringLiteral("createdAt"), profile.createdAt.toString(Qt::ISODate));
    obj.insert(QStringLiteral("updatedAt"), profile.updatedAt.toString(Qt::ISODate));
    obj.insert(QStringLiteral("passwordExported"), passwordExported);
    obj.insert(QStringLiteral("password"), QStringLiteral("not exported"));
    return obj;
}

static SipProfile profileFromJson(const QJsonObject &obj)
{
    SipProfile profile;
    profile.profileId = obj.value(QStringLiteral("profileId")).toString();
    profile.displayName = obj.value(QStringLiteral("displayName")).toString();
    profile.sipUsername = obj.value(QStringLiteral("sipUsername")).toString();
    profile.sipDomain = obj.value(QStringLiteral("sipDomain")).toString();
    profile.sipUri = obj.value(QStringLiteral("sipUri")).toString();
    profile.registrar = obj.value(QStringLiteral("registrar")).toString();
    profile.proxy = obj.value(QStringLiteral("proxy")).toString();
    profile.outboundProxy = obj.value(QStringLiteral("outboundProxy")).toString();
    profile.authUsername = obj.value(QStringLiteral("authUsername")).toString();
    profile.emergencyServiceUri = obj.value(QStringLiteral("emergencyServiceUri")).toString();
    profile.enableRtt = obj.value(QStringLiteral("enableRtt")).toBool(false);
    profile.enableLmpe = obj.value(QStringLiteral("enableLmpe")).toBool(false);
    profile.enableEtsiCompatibility = obj.value(QStringLiteral("enableEtsiCompatibility")).toBool(false);
    profile.createdAt = QDateTime::fromString(obj.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
    profile.updatedAt = QDateTime::fromString(obj.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
    bool transportOk = false;
    profile.transport = SipProfileManager::transportFromString(
        obj.value(QStringLiteral("transport")).toString(QStringLiteral("UDP")), &transportOk);
    Q_UNUSED(transportOk)
    return profile;
}

static QJsonObject exportProfilesJson()
{
    QJsonArray profiles;
    const auto items = SipProfileManager::instance().profiles();
    for (const SipProfile &profile : items)
        profiles.append(profileToJson(profile, false));

    QJsonObject obj;
    obj.insert(QStringLiteral("activeProfileId"), SipProfileManager::instance().activeProfileId());
    obj.insert(QStringLiteral("profiles"), profiles);
    return obj;
}

static bool importProfilesJson(const QJsonObject &root, QString *errorMessage)
{
    if (!root.contains(QStringLiteral("profiles")) || !root.value(QStringLiteral("profiles")).isArray()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Missing profiles array");
        return false;
    }

    const QJsonArray profiles = root.value(QStringLiteral("profiles")).toArray();
    QList<SipProfile> importedProfiles;
    for (const QJsonValue &value : profiles) {
        if (!value.isObject())
            continue;
        SipProfile profile = profileFromJson(value.toObject());
        if (profile.profileId.isEmpty())
            profile = SipProfile::createNew();
        if (profile.displayName.trimmed().isEmpty()
            || profile.sipUsername.trimmed().isEmpty()
            || profile.sipDomain.trimmed().isEmpty()
            || profile.registrar.trimmed().isEmpty()) {
            continue;
        }
        importedProfiles.append(profile);
    }

    if (importedProfiles.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No valid SIP profiles found in configuration");
        return false;
    }

    auto &mgr = SipProfileManager::instance();
    const auto existing = mgr.profiles();
    for (const SipProfile &profile : existing)
        mgr.remove(profile.profileId);

    int importedCount = 0;
    for (const SipProfile &profile : importedProfiles) {
        const QString id = mgr.add(profile);
        if (!id.isEmpty())
            ++importedCount;
    }

    const QString activeId = root.value(QStringLiteral("activeProfileId")).toString();
    if (!activeId.isEmpty() && mgr.hasProfile(activeId)) {
        mgr.setActiveProfileId(activeId);
    } else if (importedCount > 0 && !mgr.profiles().isEmpty()) {
        mgr.setActiveProfileId(mgr.profiles().first().profileId);
    } else {
        mgr.setActiveProfileId(QString{});
    }

    if (errorMessage)
        *errorMessage = QStringLiteral("%1 profile(s) imported").arg(importedCount);
    return true;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    PerfScope total("MainWindow constructor total");
    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] MainWindow constructor start at T+%1 ms")
            .arg(PerfScope::msecsSinceAppStart()));

    setWindowTitle("SIP Client - Audio / Video / RTT");
    setMinimumSize(1180, 820);
    resize(1500, 960);

    { PerfScope s("MainWindow::buildMenuBar");     buildMenuBar();     }
    { PerfScope s("MainWindow::buildCentralWidget"); buildCentralWidget(); }
    { PerfScope s("MainWindow::buildStatusBar");   buildStatusBar();   }

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            m_statusBar, &AppStatusBar::setRegistrationStatus);

    { PerfScope s("MainWindow::restoreLayout"); restoreLayout(); }

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
    m_actImportConfig = menuFile->addAction(tr("&Import configuration..."), this,
                                            &MainWindow::importConfiguration);
    m_actExportConfig = menuFile->addAction(tr("&Export configuration..."), this,
                                            &MainWindow::exportConfiguration);
    menuFile->addSeparator();
    menuFile->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    auto *menuHelp = mb->addMenu(tr("&Help"));
    m_actAbout = menuHelp->addAction(tr("&About"), this, &MainWindow::showAboutDialog);
    m_actDiagnosticsInfo = menuHelp->addAction(tr("&Diagnostics info"), this,
                                               &MainWindow::showDiagnosticsInfo);
}

QWidget *MainWindow::buildDashboardPage()
{
    m_dashboardPage = new DashboardPage(this);
    connect(m_dashboardPage, &DashboardPage::navigateTo,
            this, &MainWindow::onNavPageRequested);
    return m_dashboardPage;
}

QWidget *MainWindow::buildClientsPage()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);

    // Outer horizontal splitter: [left vertical split | RTT panel]
    auto *outerSplit = new QSplitter(Qt::Horizontal, page);
    outerSplit->setChildrenCollapsible(false);
    outerSplit->setHandleWidth(6);

    // Left vertical splitter: [VideoPanel (top 50%) | CallPanel (bottom 50%)]
    auto *leftSplit = new QSplitter(Qt::Vertical, outerSplit);
    leftSplit->setChildrenCollapsible(false);
    leftSplit->setHandleWidth(6);

    m_clientsVideoPanel = new VideoPanel(leftSplit);
    m_clientsVideoPanel->setObjectName("ClientsVideoPanel");
    leftSplit->addWidget(m_clientsVideoPanel);

    m_callPanel = new CallPanel(leftSplit);
    leftSplit->addWidget(m_callPanel);
    leftSplit->setSizes({480, 480});

    outerSplit->addWidget(leftSplit);

    m_rttPanel = new RttPanel(outerSplit);
    outerSplit->addWidget(m_rttPanel);
    outerSplit->setSizes({1100, 400});
    outerSplit->setStretchFactor(0, 3);
    outerSplit->setStretchFactor(1, 1);

    root->addWidget(outerSplit);

    // Start/Stop local video preview from the call panel buttons
    connect(m_callPanel, &CallPanel::startLocalVideoRequested,
            m_clientsVideoPanel, &VideoPanel::startIdlePreview);
    connect(m_callPanel, &CallPanel::stopLocalVideoRequested,
            m_clientsVideoPanel, &VideoPanel::stopIdlePreview);

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
    return new MediaPanel(this);
}

void MainWindow::exportConfiguration()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export configuration"),
        QStringLiteral("sipclient-config.json"),
        tr("JSON Files (*.json)"));
    if (path.isEmpty())
        return;

    QJsonObject root;
    root.insert(QStringLiteral("configVersion"), 1);
    root.insert(QStringLiteral("application"), exportAppSettings());
    root.insert(QStringLiteral("sipProfiles"), exportProfilesJson());
    root.insert(QStringLiteral("exportedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export configuration"),
                             tr("Could not open file for writing:\n%1").arg(path));
        return;
    }

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        QMessageBox::warning(this, tr("Export configuration"),
                             tr("Failed to write configuration file."));
        return;
    }

    QMessageBox::information(this, tr("Export configuration"),
                             tr("Configuration exported successfully.\n%1").arg(path));
}

void MainWindow::importConfiguration()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Import configuration"),
        QString{},
        tr("JSON Files (*.json)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import configuration"),
                             tr("Could not open file for reading:\n%1").arg(path));
        return;
    }

    const QByteArray raw = file.readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Import configuration"),
                             tr("Invalid JSON configuration file.\n%1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("configVersion")).toInt(-1) != 1) {
        QMessageBox::warning(this, tr("Import configuration"),
                             tr("Unsupported configuration version."));
        return;
    }

    importAppSettings(root.value(QStringLiteral("application")).toObject());

    QString profilesMessage;
    if (root.contains(QStringLiteral("sipProfiles"))) {
        if (!importProfilesJson(root.value(QStringLiteral("sipProfiles")).toObject(), &profilesMessage)) {
            QMessageBox::warning(this, tr("Import configuration"),
                                 tr("Configuration imported partially, but SIP profiles could not be restored.\n%1")
                                     .arg(profilesMessage));
        }
    }

    if (m_settingsPanel)
        m_settingsPanel->reload();
    // If Settings page hasn't been visited yet (still a placeholder),
    // skip reload — construction will read the latest settings on first visit.

    QMessageBox::information(this, tr("Import configuration"),
                             tr("Configuration imported successfully.\n%1").arg(path));
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("About SIP Client"),
        tr("SIP Client - Audio / Video / RTT\n"
           "Desktop Qt client for SIP communication, diagnostics and media testing."));
}

void MainWindow::showDiagnosticsInfo()
{
    const auto &sip = SipManager::instance();
    const QString info = tr(
        "Backend: %1\n"
        "Initialized: %2\n"
        "Active profile: %3\n"
        "Registered profile: %4\n"
        "Registration state: %5\n"
        "Call state: %6\n"
        "Profile count: %7\n"
        "Qt version: %8")
        .arg(sip.backendName(),
             sip.isInitialized() ? tr("yes") : tr("no"),
             SipProfileManager::instance().activeProfileId().isEmpty()
                ? tr("(none)")
                : SipProfileManager::instance().activeProfile().displayName,
             sip.registeredProfileId().isEmpty() ? tr("(none)") : sip.registeredProfileId(),
             QString::number(static_cast<int>(sip.registrationState())),
             QString::number(static_cast<int>(sip.callState())),
             QString::number(SipProfileManager::instance().profiles().size()),
             QString::fromLatin1(qVersion()));

    QMessageBox::information(this, tr("Diagnostics info"), info);
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

    // All 6 pages start as lightweight placeholders — real content is built lazily
    // on first navigation via ensurePage(). This keeps the constructor fast so
    // MainWindow::show() is called before any heavy page construction.
    static const char *const kPageNames[] = {
        "Dashboard", "Clients", "SIP Ladder", "Logs", "Media", "Settings"
    };
    for (int i = 0; i < kPageCount; ++i) {
        m_pageStack->addWidget(makePlaceholder(tr(kPageNames[i]), m_pageStack));
        m_pageBuilt[i] = false;
    }

    setCentralWidget(central);

    // Dashboard is the default visible page — build it on the first event-loop
    // tick (after show() has returned) so the placeholder is briefly visible
    // and the window paints before any heavy widget construction runs.
    QTimer::singleShot(0, this, [this]() {
        ensurePage(0);
        m_pageStack->setCurrentIndex(0);
    });
}

void MainWindow::ensurePage(int index)
{
    if (index < 0 || index >= kPageCount || m_pageBuilt[index])
        return;

    m_pageBuilt[index] = true;

    const qint64 tStart = PerfScope::msecsSinceAppStart();
    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] lazy build start: page[%1] at T+%2 ms")
            .arg(index).arg(tStart));

    QElapsedTimer t;
    t.start();

    QWidget *real = nullptr;
    switch (index) {
    case 0: real = buildDashboardPage();                         break;
    case 1: real = buildClientsPage();                           break;
    case 2:
        m_ladderPage = new SipLadderPage(m_pageStack);
        real = m_ladderPage;
        break;
    case 3: real = buildLogsPage();                              break;
    case 4: real = buildMediaPage();                             break;
    case 5:
        m_settingsPanel = new SettingsPanel(m_pageStack);
        real = m_settingsPanel;
        break;
    default:
        return;
    }

    // Replace the placeholder at 'index' with the real page.
    // insertWidget(index, real) shifts the old placeholder to index+1;
    // removeWidget then pulls it back out, restoring stable indices.
    QWidget *old = m_pageStack->widget(index);
    m_pageStack->insertWidget(index, real);
    m_pageStack->removeWidget(old);
    old->deleteLater();

    const qint64 elapsed = t.elapsed();
    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] lazy build done: page[%1] in %2 ms (T+%3 ms)")
            .arg(index).arg(elapsed).arg(PerfScope::msecsSinceAppStart()));
}

void MainWindow::showSettingsDialog()
{
    onNavPageRequested(QStringLiteral("settings"));
}

void MainWindow::onNavPageRequested(const QString &page)
{
    QString activePage = page;
    int pageIndex = -1;

    if (page == QLatin1String("dashboard")) {
        pageIndex = 0;
    } else if (page == QLatin1String("clients") || page == QLatin1String("accounts")
               || page == QLatin1String("contacts") || page == QLatin1String("dialpad")) {
        pageIndex = 1;
        activePage = QStringLiteral("clients");
    } else if (page == QLatin1String("sipladder")) {
        pageIndex = 2;
    } else if (page == QLatin1String("logs")) {
        pageIndex = 3;
    } else if (page == QLatin1String("media")) {
        pageIndex = 4;
    } else if (page == QLatin1String("settings")) {
        pageIndex = 5;
    }

    if (pageIndex >= 0) {
        Logger::instance().info(LogCategory::Perf,
            QStringLiteral("[PERF] page requested: %1 (index %2) at T+%3 ms")
                .arg(page).arg(pageIndex).arg(PerfScope::msecsSinceAppStart()));
        ensurePage(pageIndex);
        m_pageStack->setCurrentIndex(pageIndex);
    }

    if (page == QLatin1String("dialpad") && m_callPanel)
        m_callPanel->focusDialInput();

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

void MainWindow::setInitializingStatus(const QString &message)
{
    if (m_statusBar)
        m_statusBar->setSipBackend(message, false);
}
