#include "MainWindow.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "core/PerfScope.h"
#include "gui/dashboard/DashboardPage.h"
#include "gui/panels/ContactsPanel.h"
#include "gui/panels/DiagnosticsPanel.h"
#include "gui/panels/NavRail.h"
#include "gui/panels/SettingsPanel.h"
#include "gui/panels/SipLadderPage.h"
#include "gui/widgets/FlowLayout.h"
#include "gui/widgets/StatusCard.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "media/VideoQualityManager.h"
#include "media/VideoStatistics.h"
#include "gui/panels/VideoPanel.h"
#include "gui/panels/RttPanel.h"
#include "gui/widgets/AppStatusBar.h"
#include "sip/CallStateMachine.h"
#include "sip/CallMediaOptions.h"
#include "sip/SipProfileManager.h"
#include "sip/SipManager.h"
#include "sip/SipUriNormalizer.h"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QDateTime>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMenu>
#include <QMenuBar>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QList>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QGroupBox>
#include <memory>

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

    auto *split = new QSplitter(Qt::Horizontal, page);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(6);

    auto makeActionButton = [](const QString &text, QWidget *parent, bool checkable = false) {
        auto *btn = new QPushButton(text, parent);
        btn->setObjectName(QStringLiteral("CallCtrlBtn"));
        btn->setCheckable(checkable);
        btn->setMinimumHeight(32);
        return btn;
    };

    auto makeStatusCard = [](const QString &title, const QString &tip, QWidget *parent) {
        auto *card = new StatusCard(title, parent);
        card->setTooltipText(tip);
        return card;
    };

    // ------------------------------------------------------------------
    // LEFT COLUMN: call control + dialpad + contacts
    // ------------------------------------------------------------------
    auto *leftWidget = new QWidget(split);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(10);

    auto *callGroup = new QGroupBox(tr("Call Control"), leftWidget);
    auto *callLayout = new QVBoxLayout(callGroup);
    callLayout->setContentsMargins(10, 10, 10, 10);
    callLayout->setSpacing(8);

    m_clientsTargetInput = new QLineEdit(callGroup);
    m_clientsTargetInput->setObjectName(QStringLiteral("ClientsTargetEdit"));
    m_clientsTargetInput->setPlaceholderText(tr("Enter SIP URI or number"));
    m_clientsTargetInput->setClearButtonEnabled(true);
    m_clientsTargetInput->setMinimumHeight(34);
    callLayout->addWidget(m_clientsTargetInput);

    auto *callRow = new QHBoxLayout();
    callRow->setSpacing(6);
    auto *callBtn = makeActionButton(tr("Call"), callGroup);
    auto *answerBtn = makeActionButton(tr("Answer"), callGroup);
    auto *rejectBtn = makeActionButton(tr("Reject"), callGroup);
    auto *hangupBtn = makeActionButton(tr("Hangup"), callGroup);
    auto *muteBtn = makeActionButton(tr("Mute"), callGroup, true);
    auto *holdBtn = makeActionButton(tr("Hold"), callGroup, true);
    auto *requestVideoBtn = makeActionButton(tr("Request Video"), callGroup, true);
    auto *requestRttBtn = makeActionButton(tr("Request RTT"), callGroup, true);
    callBtn->setProperty("callRole", QStringLiteral("call"));
    answerBtn->setProperty("callRole", QStringLiteral("answer"));
    rejectBtn->setProperty("callRole", QStringLiteral("reject"));
    hangupBtn->setProperty("callRole", QStringLiteral("hangup"));
    muteBtn->setProperty("callRole", QStringLiteral("mute"));
    holdBtn->setProperty("callRole", QStringLiteral("hold"));
    requestVideoBtn->setProperty("callRole", QStringLiteral("requestVideo"));
    requestRttBtn->setProperty("callRole", QStringLiteral("requestRtt"));
    callRow->addWidget(callBtn);
    callRow->addWidget(answerBtn);
    callRow->addWidget(rejectBtn);
    callRow->addWidget(hangupBtn);
    callRow->addWidget(muteBtn);
    callRow->addWidget(holdBtn);
    callRow->addWidget(requestVideoBtn);
    callRow->addWidget(requestRttBtn);
    callLayout->addLayout(callRow);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(6);
    auto *clearTargetBtn = makeActionButton(tr("Clear"), callGroup);
    auto *backspaceBtn   = makeActionButton(tr("Backspace"), callGroup);
    actionRow->addWidget(clearTargetBtn);
    actionRow->addWidget(backspaceBtn);
    actionRow->addStretch(1);
    callLayout->addLayout(actionRow);

    leftLayout->addWidget(callGroup);

    auto *dialGroup = new QGroupBox(tr("Dialpad"), leftWidget);
    auto *dialGrid = new QGridLayout(dialGroup);
    dialGrid->setContentsMargins(10, 10, 10, 10);
    dialGrid->setHorizontalSpacing(6);
    dialGrid->setVerticalSpacing(6);
    const QString keys[] = {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                            QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"),
                            QStringLiteral("7"), QStringLiteral("8"), QStringLiteral("9"),
                            QStringLiteral("*"), QStringLiteral("0"), QStringLiteral("#")};
    for (int i = 0; i < 12; ++i) {
        auto *btn = makeActionButton(keys[i], dialGroup);
        btn->setMinimumSize(54, 42);
        const int row = i / 3;
        const int col = i % 3;
        dialGrid->addWidget(btn, row, col);
        connect(btn, &QPushButton::clicked, this, [this, keys, i]() {
            if (m_clientsTargetInput)
                m_clientsTargetInput->insert(keys[i]);
        });
    }
    leftLayout->addWidget(dialGroup);

    m_contactsPanel = new ContactsPanel(leftWidget);
    leftLayout->addWidget(m_contactsPanel, 1);

    split->addWidget(leftWidget);

    // ------------------------------------------------------------------
    // CENTER COLUMN: status/device controls + video PIP
    // ------------------------------------------------------------------
    auto *centerWidget = new QWidget(split);
    auto *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(12, 12, 12, 12);
    centerLayout->setSpacing(10);

    auto *statusGroup = new QGroupBox(tr("Status / Device"), centerWidget);
    auto *statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->setContentsMargins(10, 10, 10, 10);
    statusLayout->setSpacing(8);

    auto *cardsArea = new QScrollArea(statusGroup);
    cardsArea->setWidgetResizable(true);
    cardsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cardsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cardsArea->setFrameShape(QFrame::NoFrame);
    cardsArea->setMinimumHeight(160);

    auto *cardsHost = new QWidget(cardsArea);
    auto *cardsFlow = new FlowLayout(cardsHost, 4, 5, 5);
    cardsHost->setLayout(cardsFlow);

    auto *cardState       = makeStatusCard(tr("Call State"), tr("Current SIP call state"), cardsHost);
    auto *cardDuration    = makeStatusCard(tr("Duration"), tr("Elapsed call duration"), cardsHost);
    auto *cardAudio       = makeStatusCard(tr("Audio"), tr("Audio media state"), cardsHost);
    auto *cardVideo       = makeStatusCard(tr("SIP Video"), tr("Video request / media state"), cardsHost);
    auto *cardRtt         = makeStatusCard(tr("RTT"), tr("RTT request / media state"), cardsHost);
    auto *cardLmpe        = makeStatusCard(tr("LMPE"), tr("LMPE capability state"), cardsHost);
    auto *cardLocalVideo  = makeStatusCard(tr("Local Video"), tr("Local camera preview state"), cardsHost);
    auto *cardRemoteVideo = makeStatusCard(tr("Remote Video"), tr("Remote video stream state"), cardsHost);
    auto *cardVideoCodec  = makeStatusCard(tr("Video Codec"), tr("Preferred video codec"), cardsHost);
    auto *cardBitrate     = makeStatusCard(tr("Bitrate"), tr("Configured video bitrate"), cardsHost);
    auto *cardResolution  = makeStatusCard(tr("Resolution"), tr("Configured video resolution"), cardsHost);
    auto *cardFps         = makeStatusCard(tr("FPS"), tr("Local preview frame rate"), cardsHost);
    auto *cardRemoteUri   = makeStatusCard(tr("Remote URI"), tr("Remote SIP URI"), cardsHost);
    auto *cardLocalAccount= makeStatusCard(tr("Local Account"), tr("Active SIP account URI"), cardsHost);
    auto *cardPacketLoss  = makeStatusCard(tr("Packet Loss"), tr("Frame drops this second"), cardsHost);
    auto *cardJitter      = makeStatusCard(tr("Jitter"), tr("RTP jitter"), cardsHost);
    auto *cardLatency     = makeStatusCard(tr("Latency"), tr("Round-trip latency"), cardsHost);

    auto callStartTime = std::make_shared<QDateTime>();
    auto *durationTimer = new QTimer(page);
    durationTimer->setInterval(1000);

    StatusCard *allCards[] = {
        cardState, cardDuration, cardAudio, cardVideo, cardRtt, cardLmpe, cardLocalVideo,
        cardRemoteVideo, cardVideoCodec, cardBitrate, cardResolution, cardFps,
        cardRemoteUri, cardLocalAccount, cardPacketLoss, cardJitter, cardLatency
    };
    for (StatusCard *card : allCards)
        cardsFlow->addWidget(card);

    cardsArea->setWidget(cardsHost);
    statusLayout->addWidget(cardsArea);

    auto *deviceRow = new QHBoxLayout();
    deviceRow->setSpacing(6);

    auto *micLabel = new QLabel(tr("Microphone"), statusGroup);
    auto *speakerLabel = new QLabel(tr("Speaker"), statusGroup);
    auto *cameraLabel = new QLabel(tr("Camera"), statusGroup);
    auto *micCombo = new QComboBox(statusGroup);
    auto *speakerCombo = new QComboBox(statusGroup);
    auto *cameraCombo = new QComboBox(statusGroup);
    micCombo->setMinimumHeight(28);
    speakerCombo->setMinimumHeight(28);
    cameraCombo->setMinimumHeight(28);
    deviceRow->addWidget(micLabel);
    deviceRow->addWidget(micCombo, 1);
    deviceRow->addWidget(speakerLabel);
    deviceRow->addWidget(speakerCombo, 1);
    deviceRow->addWidget(cameraLabel);
    deviceRow->addWidget(cameraCombo, 1);
    statusLayout->addLayout(deviceRow);

    centerLayout->addWidget(statusGroup, 0);

    m_clientsVideoPanel = new VideoPanel(centerWidget);
    m_clientsVideoPanel->setObjectName("ClientsVideoPanel");
    m_clientsVideoPanel->setMinimumSize(360, 260);
    centerLayout->addWidget(m_clientsVideoPanel, 1);

    split->addWidget(centerWidget);

    // ------------------------------------------------------------------
    // RIGHT COLUMN: RTT / LMPE
    // ------------------------------------------------------------------
    m_rttPanel = new RttPanel(split);
    split->addWidget(m_rttPanel);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 1);
    split->setSizes(QList<int>{420, 840, 420});

    root->addWidget(split);

    // ------------------------------------------------------------------
    // Wiring
    // ------------------------------------------------------------------
    auto refreshSelectionCombos = [this, micCombo, speakerCombo, cameraCombo]() {
        const MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
        const QString curMic = sel.selectedMicrophone().id;
        const QString curSpk = sel.selectedSpeaker().id;
        const QString curCam = sel.selectedCamera().id;

        const auto fillCombo = [](QComboBox *combo, const QList<MediaDevice> &devices,
                                  const QString &currentId, const QString &defaultLabel) {
            const QSignalBlocker blocker(combo);
            combo->clear();
            combo->addItem(defaultLabel, QString{});
            for (const MediaDevice &d : devices)
                combo->addItem(d.displayName, d.id);
            for (int i = 0; i < combo->count(); ++i) {
                if (combo->itemData(i).toString() == currentId) {
                    combo->setCurrentIndex(i);
                    return;
                }
            }
        };

        fillCombo(micCombo, MediaDeviceManager::instance().listMicrophones(), curMic,
                  MainWindow::tr("Default"));
        fillCombo(speakerCombo, MediaDeviceManager::instance().listSpeakers(), curSpk,
                  MainWindow::tr("Default"));
        fillCombo(cameraCombo, MediaDeviceManager::instance().listCameras(), curCam,
                  MainWindow::tr("Default"));
    };

    refreshSelectionCombos();

    connect(m_contactsPanel, &ContactsPanel::dialRequested, this, [this](const QString &uri) {
        if (m_clientsTargetInput)
            m_clientsTargetInput->setText(uri);
    });

    connect(clearTargetBtn, &QPushButton::clicked, this, [this]() {
        if (m_clientsTargetInput)
            m_clientsTargetInput->clear();
    });
    connect(backspaceBtn, &QPushButton::clicked, this, [this]() {
        if (m_clientsTargetInput)
            m_clientsTargetInput->backspace();
    });

    auto refreshHoldButton = [holdBtn]() {
        const CallState state = SipManager::instance().callState();
        const bool held = (state == CallState::Held);
        const bool active = (state == CallState::Active || state == CallState::Held);
        QSignalBlocker blocker(holdBtn);
        holdBtn->setEnabled(active);
        holdBtn->setChecked(held);
        holdBtn->setText(held ? MainWindow::tr("Unhold") : MainWindow::tr("Hold"));
    };

    auto videoRequestFailed = std::make_shared<bool>(false);
    auto rttRequestFailed = std::make_shared<bool>(false);

    auto holdConfirmTimer = new QTimer(page);
    holdConfirmTimer->setSingleShot(true);
    holdConfirmTimer->setInterval(1200);

    connect(callBtn, &QPushButton::clicked, this, [this, requestVideoBtn, requestRttBtn]() {
        const QString raw = m_clientsTargetInput ? m_clientsTargetInput->text().trimmed() : QString{};
        if (raw.isEmpty())
            return;
        const QString fallbackDomain = SipProfileManager::instance().activeProfile().sipDomain;
        const SipUriNormalizer::Result result = SipUriNormalizer::normalize(raw, fallbackDomain);
        if (!result.isValid) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Dial URI invalid: input=\"%1\" error=\"%2\"")
                    .arg(raw, result.error));
            return;
        }
        if (m_clientsTargetInput && result.uri != raw)
            m_clientsTargetInput->setText(result.uri);
        CallType callType = CallType::AudioOnly;
        if (requestVideoBtn->isChecked() && requestRttBtn->isChecked())
            callType = CallType::AudioVideoRtt;
        else if (requestVideoBtn->isChecked())
            callType = CallType::AudioVideo;
        else if (requestRttBtn->isChecked())
            callType = CallType::AudioRtt;
        const CallMediaOptions opts = CallMediaOptions::fromType(callType);
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Placing call from Clients layout: uri=%1 type=%2")
                .arg(result.uri, callTypeName(callType)));
        SipManager::instance().makeCall(result.uri, opts);
    });
    connect(answerBtn, &QPushButton::clicked, this, []() { SipManager::instance().answerCall(); });
    connect(rejectBtn, &QPushButton::clicked, this, []() { SipManager::instance().rejectCall(); });
    connect(hangupBtn, &QPushButton::clicked, this, []() { SipManager::instance().hangupCall(); });
    connect(muteBtn, &QPushButton::toggled, this, [](bool on) {
        SipManager::instance().setCallMuted(on);
    });
    connect(holdBtn, &QPushButton::toggled, this, [this, holdBtn, holdConfirmTimer, refreshHoldButton](bool on) {
        Logger::instance().info(LogCategory::Sip,
            on ? QStringLiteral("Hold requested") : QStringLiteral("Unhold requested"));
        holdBtn->setText(on ? tr("Hold...") : tr("Unhold..."));
        holdConfirmTimer->start();
        const bool ok = on ? SipManager::instance().holdCall() : SipManager::instance().resumeCall();
        if (!ok) {
            holdConfirmTimer->stop();
            Logger::instance().warn(LogCategory::Sip, QStringLiteral("Hold failed"));
            refreshHoldButton();
        }
    });

    connect(holdConfirmTimer, &QTimer::timeout, this, [this, holdBtn, refreshHoldButton]() {
        const CallState state = SipManager::instance().callState();
        const bool desiredHold = holdBtn->isChecked();
        const bool confirmed = desiredHold ? (state == CallState::Held)
                                           : (state == CallState::Active);
        if (confirmed) {
            Logger::instance().info(LogCategory::Sip,
                desiredHold ? QStringLiteral("Hold active") : QStringLiteral("Hold released"));
            refreshHoldButton();
            return;
        }

        Logger::instance().warn(LogCategory::Sip, QStringLiteral("Hold failed"));
        QSignalBlocker blocker(holdBtn);
        holdBtn->setChecked(state == CallState::Held);
        refreshHoldButton();
    });

    connect(requestVideoBtn, &QPushButton::toggled, this,
            [this, cardVideo, requestVideoBtn, videoRequestFailed](bool enabled) {
        *videoRequestFailed = false;
        cardVideo->setValue(enabled ? tr("Requested") : tr("Not requested"));
        cardVideo->setStatus(enabled ? QStringLiteral("warn") : QString{});
        if (SipManager::instance().callState() == CallState::Idle
            || SipManager::instance().callState() == CallState::Failed) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Request Video %1 will apply on next call")
                    .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF")));
            return;
        }
        if (!SipManager::instance().requestCallVideo(enabled)) {
            *videoRequestFailed = true;
            cardVideo->setValue(tr("Failed"));
            cardVideo->setStatus(QStringLiteral("err"));
        }
    });
    connect(requestRttBtn, &QPushButton::toggled, this,
            [this, cardRtt, requestRttBtn, rttRequestFailed](bool enabled) {
        *rttRequestFailed = false;
        cardRtt->setValue(enabled ? tr("Requested") : tr("Not requested"));
        cardRtt->setStatus(enabled ? QStringLiteral("warn") : QString{});
        if (SipManager::instance().callState() == CallState::Idle
            || SipManager::instance().callState() == CallState::Failed) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("RTT request will apply on next call"));
            return;
        }
        if (!SipManager::instance().requestCallRtt(enabled)) {
            *rttRequestFailed = true;
            cardRtt->setValue(tr("Failed"));
            cardRtt->setStatus(QStringLiteral("err"));
        }
    });

    connect(micCombo, &QComboBox::currentIndexChanged, this, [micCombo](int idx) {
        AudioMediaManager::instance().setMicrophone(micCombo->itemData(idx).toString());
    });
    connect(speakerCombo, &QComboBox::currentIndexChanged, this, [speakerCombo](int idx) {
        AudioMediaManager::instance().setSpeaker(speakerCombo->itemData(idx).toString());
    });
    connect(cameraCombo, &QComboBox::currentIndexChanged, this, [cameraCombo](int idx) {
        VideoMediaManager::instance().setCamera(cameraCombo->itemData(idx).toString());
    });

    auto refreshCards = [=]() {
        const CallState state = SipManager::instance().callState();
        const QString statusText = SipManager::instance().callStatusText();
        const bool active = (state == CallState::Active || state == CallState::Held);
        const bool audioActive = (state != CallState::Idle && state != CallState::Failed);
        const bool videoActive = VideoMediaManager::instance().isVideoActive();
        const bool localVideo = VideoMediaManager::instance().isLocalVideoAvailable();
        const bool remoteVideo = VideoMediaManager::instance().isRemoteVideoAvailable();
        const QString remoteUri = SipManager::instance().activeCallRemoteUri();
        const QString localAccount = SipProfileManager::instance().activeProfile().isNull()
            ? QString{} : SipProfileManager::instance().activeProfile().effectiveSipUri();
        const VideoSettings vs = VideoQualityManager::instance().current();

        cardState->setValue(statusText.isEmpty() ? tr("—") : statusText);
        cardState->setStatus(active ? QStringLiteral("ok")
                                    : (state == CallState::Idle ? QString{} : QStringLiteral("warn")));

        if (state == CallState::Active)
            cardDuration->setStatus(QStringLiteral("ok"));
        else if (state == CallState::Held)
            cardDuration->setStatus(QStringLiteral("warn"));
        else
            cardDuration->setStatus({});

        cardDuration->setValue((state == CallState::Active || state == CallState::Held)
                                   ? QStringLiteral("00:00:00")
                                   : QStringLiteral("—"));
        cardAudio->setValue(audioActive ? tr("Connected") : QStringLiteral("—"));
        cardAudio->setStatus(audioActive ? QStringLiteral("ok") : QString{});
        if (*videoRequestFailed) {
            cardVideo->setValue(tr("Failed"));
            cardVideo->setStatus(QStringLiteral("err"));
        } else {
            cardVideo->setValue(videoActive
                                    ? tr("Active")
                                    : (requestVideoBtn->isChecked()
                                           ? tr("Requested")
                                           : tr("Not requested")));
            cardVideo->setStatus(videoActive ? QStringLiteral("ok")
                                             : (requestVideoBtn->isChecked()
                                                    ? QStringLiteral("warn")
                                                    : QString{}));
        }
        if (*rttRequestFailed) {
            cardRtt->setValue(tr("Failed"));
            cardRtt->setStatus(QStringLiteral("err"));
        } else {
            cardRtt->setValue(SipManager::instance().rttSession()->isActive()
                                  ? tr("Active")
                                  : (requestRttBtn->isChecked()
                                         ? tr("Requested")
                                         : tr("Not requested")));
            cardRtt->setStatus(SipManager::instance().rttSession()->isActive()
                                   ? QStringLiteral("ok")
                                   : (requestRttBtn->isChecked()
                                          ? QStringLiteral("warn")
                                          : QString{}));
        }
        cardLmpe->setValue(QStringLiteral("—"));
        cardLmpe->setStatus({});
        cardLocalVideo->setValue(localVideo ? tr("On") : tr("Off"));
        cardLocalVideo->setStatus(localVideo ? QStringLiteral("ok") : QString{});
        cardRemoteVideo->setValue(remoteVideo ? tr("On") : tr("Off"));
        cardRemoteVideo->setStatus(remoteVideo ? QStringLiteral("ok") : QString{});
        if (videoActive || localVideo) {
            cardVideoCodec->setValue(vs.codecOrder.isEmpty() ? QStringLiteral("—")
                                                             : vs.codecOrder.first());
            cardVideoCodec->setStatus(QStringLiteral("ok"));
            cardBitrate->setValue(QStringLiteral("%1 kbps").arg(vs.bitrateKbps));
            cardBitrate->setStatus(QStringLiteral("ok"));
            cardResolution->setValue(QStringLiteral("%1x%2")
                                         .arg(vs.resolution.width())
                                         .arg(vs.resolution.height()));
            cardResolution->setStatus(QStringLiteral("ok"));
            cardFps->setValue(QStringLiteral("%1 fps").arg(QString::number(vs.fps, 'f', 1)));
        } else {
            cardVideoCodec->setValue(QStringLiteral("—"));
            cardVideoCodec->setStatus({});
            cardBitrate->setValue(QStringLiteral("—"));
            cardBitrate->setStatus({});
            cardResolution->setValue(QStringLiteral("—"));
            cardResolution->setStatus({});
            cardFps->setValue(QStringLiteral("—"));
        }
        cardFps->setStatus((videoActive || localVideo)
                               ? QStringLiteral("ok")
                               : QString{});
        cardRemoteUri->setValue(remoteUri.isEmpty() ? QStringLiteral("—") : remoteUri);
        cardRemoteUri->setStatus(remoteUri.isEmpty() ? QString{} : QStringLiteral("ok"));
        cardLocalAccount->setValue(localAccount.isEmpty() ? QStringLiteral("—") : localAccount);
        cardLocalAccount->setStatus(localAccount.isEmpty() ? QString{} : QStringLiteral("ok"));
        cardPacketLoss->setValue(QStringLiteral("—"));
        cardJitter->setValue(QStringLiteral("—"));
        cardLatency->setValue(QStringLiteral("—"));
    };

    connect(&SipManager::instance(), &SipManager::callStateChanged,
            this, [=](CallState state, const QString &, int) {
        const bool hasCall = (state != CallState::Idle && state != CallState::Failed);
        answerBtn->setVisible(state == CallState::IncomingRinging);
        rejectBtn->setVisible(state == CallState::IncomingRinging);
        hangupBtn->setVisible(hasCall && state != CallState::IncomingRinging);
        holdBtn->setEnabled(state == CallState::Active || state == CallState::Held);
        muteBtn->setEnabled(state == CallState::Active);
        callBtn->setEnabled(SipManager::instance().registrationState() == RegistrationState::Registered
                            && (state == CallState::Idle || state == CallState::Failed));
        if (state == CallState::Idle || state == CallState::Failed)
            durationTimer->stop();
        if (state == CallState::Idle || state == CallState::Failed)
            *callStartTime = QDateTime();
        if (state == CallState::Active || state == CallState::Held)
            holdConfirmTimer->stop();
        refreshHoldButton();
        cardState->setValue(callStateDisplayText(state));
        refreshCards();
    });

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, [=](RegistrationState state, const QString &, int) {
        callBtn->setEnabled(state == RegistrationState::Registered);
        refreshCards();
    });

    connect(&SipManager::instance(), &SipManager::callConnected,
            this, [=](const QString &) { refreshCards(); });
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, [=](const QString &, const QString &, int) { refreshCards(); });
    connect(&SipManager::instance(), &SipManager::callFailed,
            this, [=](const QString &, const QString &, int) { refreshCards(); });
    connect(&SipManager::instance(), &SipManager::audioMediaConnected,
            this, [=]() { refreshCards(); });
    connect(&SipManager::instance(), &SipManager::audioMediaDisconnected,
            this, [=]() { refreshCards(); });
    connect(&SipManager::instance(), &SipManager::videoMediaConnected,
            this, [=]() { *videoRequestFailed = false; refreshCards(); });
    connect(&SipManager::instance(), &SipManager::videoMediaDisconnected,
            this, [=]() { refreshCards(); });
    connect(&SipManager::instance(), &SipManager::rttMediaConnected,
            this, [=]() { *rttRequestFailed = false; refreshCards(); });
    connect(&SipManager::instance(), &SipManager::rttMediaDisconnected,
            this, [=]() { refreshCards(); });
    connect(&VideoStatistics::instance(), &VideoStatistics::statsUpdated,
            this, [=](float fps, int drops) {
        if (VideoMediaManager::instance().isVideoActive() || VideoMediaManager::instance().isLocalVideoAvailable()) {
            cardFps->setValue(QStringLiteral("%1 fps").arg(QString::number(fps, 'f', 1)));
            cardFps->setStatus(QStringLiteral("ok"));
            cardPacketLoss->setValue(QStringLiteral("%1").arg(drops));
            cardPacketLoss->setStatus(drops > 0 ? QStringLiteral("warn") : QStringLiteral("ok"));
        }
    });
    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::devicesChanged,
            this, [=]() { refreshSelectionCombos(); refreshCards(); });
    connect(&VideoMediaManager::instance(), &VideoMediaManager::cameraChanged,
            this, [=]() { refreshSelectionCombos(); });

    connect(m_contactsPanel, &ContactsPanel::dialRequested,
            this, [this, callBtn](const QString &uri) {
        if (m_clientsTargetInput)
            m_clientsTargetInput->setText(uri);
        if (callBtn->isEnabled())
            callBtn->click();
    });

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, [=](RegistrationState state, const QString &, int) {
        if (m_clientsTargetInput)
            m_clientsTargetInput->setEnabled(state == RegistrationState::Registered);
    });

    if (m_clientsTargetInput) {
        connect(m_clientsTargetInput, &QLineEdit::returnPressed,
                callBtn, &QPushButton::click);
    }

    const bool registeredNow = (SipManager::instance().registrationState() == RegistrationState::Registered);
    callBtn->setEnabled(registeredNow);
    if (m_clientsTargetInput)
        m_clientsTargetInput->setEnabled(registeredNow);
    refreshHoldButton();

    connect(&SipManager::instance(), &SipManager::callConnected,
            this, [=](const QString &) mutable {
        *callStartTime = QDateTime::currentDateTime();
        durationTimer->start();
    });
    connect(durationTimer, &QTimer::timeout, this, [=]() mutable {
        if (!callStartTime->isValid()) {
            cardDuration->setValue(QStringLiteral("00:00:00"));
            return;
        }
        const int secs = callStartTime->secsTo(QDateTime::currentDateTime());
        const int h = secs / 3600;
        const int m = (secs % 3600) / 60;
        const int s = secs % 60;
        cardDuration->setValue(QStringLiteral("%1:%2:%3")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0')));
    });

    refreshCards();
    QTimer::singleShot(0, page, refreshCards);
    m_rttPanel->setRttSession(SipManager::instance().rttSession());
    return page;
}

QWidget *MainWindow::buildLogsPage()
{
    m_diagnostics = new DiagnosticsPanel(this);
    return m_diagnostics;
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
        "Dashboard", "Clients", "SIP Ladder", "Logs", "Settings"
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
    case 4:
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
    } else if (page == QLatin1String("settings")) {
        pageIndex = 4;
    } else if (page == QLatin1String("settings-video")) {
        pageIndex = 4;
        activePage = QStringLiteral("settings");
    }

    if (pageIndex >= 0) {
        Logger::instance().info(LogCategory::Perf,
            QStringLiteral("[PERF] page requested: %1 (index %2) at T+%3 ms")
                .arg(page).arg(pageIndex).arg(PerfScope::msecsSinceAppStart()));
        ensurePage(pageIndex);
        m_pageStack->setCurrentIndex(pageIndex);
        if (page == QLatin1String("settings-video") && m_settingsPanel)
            m_settingsPanel->focusVideoTab();
    }

    if (page == QLatin1String("dialpad") && m_clientsTargetInput)
        m_clientsTargetInput->setFocus();

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
