#include "MainWindow.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
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
#include <QHeaderView>
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
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>

namespace {
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
        tr("Three-zone workspace for live video, call control, audio status, and RTT."),
        page);
    desc->setStyleSheet("color: #b7c4d6;");
    desc->setWordWrap(true);
    root->addWidget(desc);

    auto *split = new QSplitter(Qt::Horizontal, page);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);

    m_clientsVideoPanel = new VideoPanel(split);
    m_clientsVideoPanel->setObjectName("ClientsVideoPanel");
    split->addWidget(m_clientsVideoPanel);

    auto *centerWrap = new QWidget(split);
    auto *centerLayout = new QVBoxLayout(centerWrap);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);
    m_callPanel = new CallPanel(centerWrap);
    centerLayout->addWidget(m_callPanel, 2);
    split->addWidget(centerWrap);

    m_rttPanel = new RttPanel(split);
    split->addWidget(m_rttPanel);

    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 1);
    split->setSizes(QList<int>{520, 520, 360});

    root->addWidget(split, 1);

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
