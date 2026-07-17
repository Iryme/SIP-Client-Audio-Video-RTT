#include "MainWindow.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "core/PerfScope.h"
#include "gui/dashboard/DashboardPage.h"
#include "gui/panels/CallHistoryPanel.h"
#include "gui/panels/ContactsPanel.h"
#include "gui/panels/NavRail.h"
#include "gui/panels/SettingsPanel.h"
#include "gui/panels/ToolsPage.h"
#include "gui/panels/ConversationWorkspacePanel.h"
#include "gui/panels/CallWorkspacePanel.h"
#include "gui/panels/messaging/ClientMessagingController.h"
#include "gui/panels/messaging/ClientMessagingView.h"
#include "AppVersion.h"
#include "gui/widgets/AudioLevelMeter.h"
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
#include "gui/CameraController.h"
#include "gui/dialogs/IncomingCallDialog.h"
#include "gui/dialogs/MediaRequestDialog.h"
#include "gui/widgets/AppStatusBar.h"
#include "rtt/RttSession.h"
#include "sip/CallStateMachine.h"
#include "sip/CallMediaOptions.h"
#include "sip/SipProfileManager.h"
#include "sip/SipManager.h"
#include "sip/SipUriNormalizer.h"

#include <QApplication>
#include <QAbstractSocket>
#include <QCoreApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QDateTime>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QHostAddress>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QNetworkInterface>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QList>
#include <QEventLoop>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QGroupBox>
#include <QTabWidget>
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

static QString localIpSourceText(QString *ipOut)
{
    auto pickIp = [](bool requireUpAndRunning, QString *ifaceName) -> QString {
        const auto interfaces = QNetworkInterface::allInterfaces();
        QString fallbackIp;
        QString fallbackName;

        for (const QNetworkInterface &iface : interfaces) {
            const auto flags = iface.flags();
            if (flags.testFlag(QNetworkInterface::IsLoopBack))
                continue;
            if (requireUpAndRunning
                && (!flags.testFlag(QNetworkInterface::IsUp)
                    || !flags.testFlag(QNetworkInterface::IsRunning))) {
                continue;
            }

            for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                const QHostAddress addr = entry.ip();
                if (addr.protocol() != QAbstractSocket::IPv4Protocol || addr.isLoopback())
                    continue;
                if (ifaceName)
                    *ifaceName = iface.humanReadableName();
                return addr.toString();
            }

            if (fallbackIp.isEmpty()) {
                for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                    const QHostAddress addr = entry.ip();
                    if (addr.protocol() != QAbstractSocket::IPv4Protocol || addr.isLoopback())
                        continue;
                    fallbackIp = addr.toString();
                    fallbackName = iface.humanReadableName();
                    break;
                }
            }
        }

        if (!fallbackIp.isEmpty() && ifaceName)
            *ifaceName = fallbackName;
        return fallbackIp;
    };

    QString ifaceName;
    QString ip = pickIp(true, &ifaceName);
    if (!ip.isEmpty()) {
        if (ipOut)
            *ipOut = ip;
        return ifaceName.isEmpty()
            ? QStringLiteral("local network interface")
            : QStringLiteral("local network interface %1").arg(ifaceName);
    }

    ip = pickIp(false, &ifaceName);
    if (!ip.isEmpty()) {
        if (ipOut)
            *ipOut = ip;
        return ifaceName.isEmpty()
            ? QStringLiteral("fallback network interface")
            : QStringLiteral("fallback network interface %1").arg(ifaceName);
    }

    if (ipOut)
        ipOut->clear();
    return QStringLiteral("no non-loopback IPv4 interface available");
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

    // Remove the old menu bar — all actions live in the NavRail sidebar now.
    setMenuBar(nullptr);
    { PerfScope s("MainWindow::buildCentralWidget"); buildCentralWidget(); }
    { PerfScope s("MainWindow::buildStatusBar");   buildStatusBar();   }

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            m_statusBar, &AppStatusBar::setRegistrationStatus);
    connect(&SipManager::instance(), &SipManager::callStateChanged,
            this, [this](CallState, const QString &, int) { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::callConnected,
            this, [this](const QString &) { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, [this](const QString &, const QString &, int) { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::callFailed,
            this, [this](const QString &, const QString &, int) { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::audioMediaConnected,
            this, [this]() { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::audioMediaDisconnected,
            this, [this]() { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::videoMediaConnected,
            this, [this]() { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::videoMediaDisconnected,
            this, [this]() { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::rttMediaConnected,
            this, [this]() { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::rttMediaDisconnected,
            this, [this]() { refreshStatusBarMetrics(); });
    connect(&SipManager::instance(), &SipManager::rtpStatsChanged,
            m_statusBar, &AppStatusBar::setRtpStats);
    if (auto *rttSession = SipManager::instance().rttSession()) {
        connect(rttSession, &RttSession::rttStateChanged,
                this, [this](RttState) { refreshStatusBarMetrics(); });
    }
    m_statusBar->setRtpStats(SipManager::instance().currentRtpStats());

    // SIP backend ready/stopped → update backend label live.
    connect(&SipManager::instance(), &SipManager::initialized,
            this, &MainWindow::updateSipBackendStatus);
    connect(&SipManager::instance(), &SipManager::shutdownComplete,
            this, &MainWindow::updateSipBackendStatus);

    // Active account + transport in the status bar
    auto refreshStatusBarAccount = [this]() {
        const QString regId    = SipManager::instance().registeredProfileId();
        const QString activeId = SipProfileManager::instance().activeProfileId();
        const QString useId    = !regId.isEmpty() ? regId : activeId;
        const SipProfile p     = SipProfileManager::instance().profile(useId);
        if (p.isNull()) {
            m_statusBar->setActiveAccount(tr("No account"));
            m_statusBar->setTransport(QStringLiteral("—"));
        } else {
            m_statusBar->setActiveAccount(p.effectiveSipUri());
            m_statusBar->setTransport(SipProfileManager::transportToString(p.transport));
        }
        refreshStatusBarMetrics();
    };
    connect(&SipProfileManager::instance(), &SipProfileManager::activeProfileChanged,
            this, [refreshStatusBarAccount](const QString &) { refreshStatusBarAccount(); });
    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, [refreshStatusBarAccount](RegistrationState, const QString &, int) {
        refreshStatusBarAccount();
    });
    refreshStatusBarAccount();
    refreshStatusBarMetrics();

    // Incoming call popup — visible regardless of active tab/page.
    m_incomingCallDialog = new IncomingCallDialog(this);
    connect(&SipManager::instance(), &SipManager::incomingCall,
            this, [this](const QString &remoteUri) {
        m_incomingCallDialog->setRemoteUri(remoteUri);
        // Position the popup in the top-right corner of the main window.
        const QRect wr = geometry();
        m_incomingCallDialog->move(
            wr.right() - m_incomingCallDialog->width() - 16,
            wr.top() + 48);
        m_incomingCallDialog->show();
        m_incomingCallDialog->raise();
        m_incomingCallDialog->activateWindow();
    });
    connect(&SipManager::instance(), &SipManager::callStateChanged,
            m_incomingCallDialog, &IncomingCallDialog::onCallStateChanged);

    // Media request popup — shown for incoming video and RTT requests regardless of active tab.
    m_mediaRequestDialog = new MediaRequestDialog(this);
    connect(&SipManager::instance(), &SipManager::videoRequested,
            this, [this]() {
        const QString remoteUri = SipManager::instance().activeCallRemoteUri();
        m_mediaRequestDialog->showVideoRequest(remoteUri);
        const QRect wr = geometry();
        m_mediaRequestDialog->move(
            wr.right() - m_mediaRequestDialog->width() - 16,
            wr.top() + 48 + (m_incomingCallDialog->isVisible()
                             ? m_incomingCallDialog->height() + 8 : 0));
    });
    connect(&SipManager::instance(), &SipManager::rttRequested,
            this, [this]() {
        const QString remoteUri = SipManager::instance().activeCallRemoteUri();
        m_mediaRequestDialog->showRttRequest(remoteUri);
        const QRect wr = geometry();
        m_mediaRequestDialog->move(
            wr.right() - m_mediaRequestDialog->width() - 16,
            wr.top() + 48 + (m_incomingCallDialog->isVisible()
                             ? m_incomingCallDialog->height() + 8 : 0));
    });
    connect(&SipManager::instance(), &SipManager::callStateChanged,
            m_mediaRequestDialog, &MediaRequestDialog::onCallStateChanged);
    connect(&SipManager::instance(), &SipManager::videoMediaConnected,
            m_mediaRequestDialog, &MediaRequestDialog::onVideoMediaConnected);
    connect(&SipManager::instance(), &SipManager::rttMediaConnected,
            m_mediaRequestDialog, &MediaRequestDialog::onRttMediaConnected);
    connect(&SipManager::instance(), &SipManager::rttRequestWithdrawn,
            m_mediaRequestDialog, &MediaRequestDialog::onRttRequestWithdrawn);
    connect(&SipManager::instance(), &SipManager::videoMediaDisconnected,
            m_mediaRequestDialog, [this]() {
        // Peer withdrew video — dismiss if we were showing a video request popup
        if (m_mediaRequestDialog->isVisible())
            m_mediaRequestDialog->hide();
    });

    // Camera on/off must mute/unmute PJSIP video transmit regardless of which
    // page is visible.  This connection lives here (not inside buildClientsPage)
    // so it is always active — buildClientsPage is lazily built on first visit.
    connect(&CameraController::instance(), &CameraController::enabledChanged,
            this, [](bool enabled) {
        SipManager::instance().setCallVideoMuted(!enabled);
    });
    // When a video stream first becomes active, sync the current camera state
    // in case the user had turned the camera off before the stream negotiated.
    connect(&SipManager::instance(), &SipManager::videoMediaConnected,
            this, []() {
        SipManager::instance().setCallVideoMuted(
            !CameraController::instance().isEnabled());
    });


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
    if (m_shutdownRequested) {
        event->accept();
        return;
    }

    event->ignore();
    requestApplicationShutdown();
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

    m_clientsSplitter = new QSplitter(Qt::Horizontal, page);
    auto *split = m_clientsSplitter;
    split->setChildrenCollapsible(false);
    split->setHandleWidth(6);

    // Constructed early (Task W112) so the left column's
    // ConversationWorkspacePanel can share its ClientMessagingController
    // (and therefore its single ConversationModel instance) — actually
    // placed into the RIGHT COLUMN's tab widget further down (Task W113a).
    m_clientMessagingView = new ClientMessagingView(split);

    auto makeActionButton = [](const QString &text, QWidget *parent, bool checkable = false) {
        auto *btn = new QPushButton(text, parent);
        btn->setObjectName(QStringLiteral("CallCtrlBtn"));
        btn->setCheckable(checkable);
        btn->setMinimumHeight(40);
        btn->setMinimumWidth(0);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return btn;
    };

    // ------------------------------------------------------------------
    // LEFT COLUMN: dial target + Conversations/Contacts (tabbed)
    //
    // Task W113a (layout/usability pass): the numeric dialpad and the
    // separate "Call Control" group box are gone — SIP URIs/numbers are
    // typed directly into the shared target input below, and it no longer
    // needs a titled box of its own. Conversations and Contacts (both
    // contact-selection surfaces, previously stacked on top of each other
    // fighting for vertical space) are now two tabs of one QTabWidget, each
    // getting the tab widget's full height instead of a fraction of a
    // stacked column.
    // ------------------------------------------------------------------
    auto *leftContainer = new QWidget(split);
    auto *leftContainerLayout = new QVBoxLayout(leftContainer);
    leftContainerLayout->setContentsMargins(8, 8, 8, 8);
    leftContainerLayout->setSpacing(6);

    m_clientsTargetInput = new QLineEdit(leftContainer);
    m_clientsTargetInput->setObjectName(QStringLiteral("ClientsTargetEdit"));
    m_clientsTargetInput->setPlaceholderText(tr("Enter SIP URI or number"));
    m_clientsTargetInput->setClearButtonEnabled(true);
    m_clientsTargetInput->setMinimumHeight(34);
    leftContainerLayout->addWidget(m_clientsTargetInput);

    auto *targetActionRow = new QHBoxLayout();
    targetActionRow->setSpacing(6);
    auto *clearTargetBtn = makeActionButton(tr("Clear"), leftContainer);
    auto *backspaceBtn   = makeActionButton(tr("Backspace"), leftContainer);
    targetActionRow->addWidget(clearTargetBtn);
    targetActionRow->addWidget(backspaceBtn);
    targetActionRow->addStretch(1);
    leftContainerLayout->addLayout(targetActionRow);

    auto *leftTabs = new QTabWidget(leftContainer);
    leftTabs->setObjectName(QStringLiteral("ClientsLeftTabs"));
    leftContainerLayout->addWidget(leftTabs, 1);

    m_conversationWorkspacePanel =
        new ConversationWorkspacePanel(m_clientMessagingView->controller(), leftTabs);
    leftTabs->addTab(m_conversationWorkspacePanel, tr("Conversations"));

    m_contactsPanel = new ContactsPanel(leftTabs);
    leftTabs->addTab(m_contactsPanel, tr("Contacts"));

    connect(m_conversationWorkspacePanel, &ConversationWorkspacePanel::conversationSelected,
            this, [this](const QString &peerUri) {
        m_clientsTargetInput->setText(peerUri);
        m_clientMessagingView->setPeerUri(peerUri);
    });
    connect(m_conversationWorkspacePanel, &ConversationWorkspacePanel::callRequested,
            this, [this](const QString &peerUri) {
        if (m_callWorkspacePanel)
            m_callWorkspacePanel->placeCall(peerUri);
    });

    split->addWidget(leftContainer);

    // ------------------------------------------------------------------
    // CENTER COLUMN: status/device controls + video PIP
    // ------------------------------------------------------------------
    auto *centerWidget = new QWidget(split);
    auto *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(12, 12, 12, 12);
    centerLayout->setSpacing(10);

    // Task W113: every call control (header, media status grid, device
    // status, jitter/loss/RTT stats, selected/negotiated media, the
    // emergency-call test-mode section) is now consolidated into
    // CallWorkspacePanel, a single source of truth (CallInfoModel) instead
    // of the ad hoc lambdas previously inlined here. It reads/writes the
    // shared dial target input so calls launched from the dialpad,
    // conversation list, call history, or contacts all go through the same
    // normalization + CallMediaOptions path (placeCall()).
    m_callWorkspacePanel = new CallWorkspacePanel(m_clientsTargetInput, centerWidget);
    connect(m_callWorkspacePanel, &CallWorkspacePanel::statusMessageRequested,
            this, [this](const QString &message, int timeoutMs) {
        if (m_statusBar)
            m_statusBar->showMessage(message, timeoutMs);
    });
    connect(m_callWorkspacePanel, &CallWorkspacePanel::openSipLadderRequested,
            this, [this](const QString &callId) {
        onNavPageRequested(QStringLiteral("sipladder"));
        if (m_toolsPage)
            m_toolsPage->filterSipLadderByCallId(callId);
    });
    centerLayout->addWidget(m_callWorkspacePanel, 0);

    m_clientsVideoPanel = new VideoPanel(centerWidget);
    m_clientsVideoPanel->setObjectName("ClientsVideoPanel");
    m_clientsVideoPanel->setMinimumSize(360, 260);
    centerLayout->addWidget(m_clientsVideoPanel, 1);

    split->addWidget(centerWidget);

    // ------------------------------------------------------------------
    // RIGHT COLUMN: Messaging / RTT (tabbed)
    //
    // Task W113a: these were two separate always-visible columns. Folded
    // into one QTabWidget — both keep receiving live signal updates while
    // their tab isn't the active one (Qt doesn't suspend hidden widgets'
    // slots), so nothing about their behavior changes, only how much
    // screen width the page needs at once.
    // ------------------------------------------------------------------
    auto *rightTabs = new QTabWidget(split);
    rightTabs->setObjectName(QStringLiteral("ClientsRightTabs"));

    // m_clientMessagingView was constructed earlier (Task W112, see above
    // in this function) so ConversationWorkspacePanel could share its
    // ClientMessagingController; it's placed into its tab here.
    rightTabs->addTab(m_clientMessagingView, tr("Messaging"));

    m_rttPanel = new RttPanel(rightTabs);
    rightTabs->addTab(m_rttPanel, tr("RTT"));

    split->addWidget(rightTabs);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 1);

    // Restore persisted column widths; fall back to default proportions.
    // QSplitter::restoreState() returns false (and leaves sizes untouched)
    // when the saved state's widget count doesn't match the current
    // splitter — which is always true the first time a user opens this
    // page after the W113a layout change (the old state was saved for 4
    // columns, this splitter now has 3), so fall back to the new default
    // sizes in that case instead of ending up with an unproportioned/empty
    // restore.
    const QByteArray savedSplit = AppSettings::loadSplitterState(QStringLiteral("clients"));
    if (savedSplit.isEmpty() || !split->restoreState(savedSplit))
        split->setSizes(QList<int>{300, 700, 380});

    // Minimum widths prevent columns from collapsing to nothing
    if (split->widget(0)) split->widget(0)->setMinimumWidth(260);
    if (split->widget(1)) split->widget(1)->setMinimumWidth(300);
    if (split->widget(2)) split->widget(2)->setMinimumWidth(320);

    connect(split, &QSplitter::splitterMoved, this, [this]() {
        if (m_clientsSplitter)
            AppSettings::saveSplitterState(QStringLiteral("clients"),
                                           m_clientsSplitter->saveState());
    });

    root->addWidget(split);

    // The Client Messaging View follows the same dial-target concept as the
    // call controls (m_clientsTargetInput is also updated to the remote URI
    // on incoming/connected calls elsewhere in this function), so messaging
    // never needs a second, separate "who am I talking to" field.
    connect(m_clientsTargetInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_clientMessagingView)
            m_clientMessagingView->setPeerUri(text);
    });


    // ------------------------------------------------------------------
    // Wiring
    // ------------------------------------------------------------------
    // Task W113: device-combo population, call-control button behavior,
    // status-card refresh, and all SipManager/VideoMediaManager/RttSession
    // signal handling now live inside CallWorkspacePanel (constructed
    // above, in the CENTER COLUMN section) — it is self-contained and
    // connects to those singletons itself. What remains here is only what
    // MainWindow itself must still own: the shared dial target input's
    // clear/backspace helpers, and the other call-launch entrypoints
    // (contacts, call history, conversation list) routed through
    // CallWorkspacePanel::placeCall() so they all get the same URI
    // normalization + CallMediaOptions + status-bar-feedback behavior the
    // dialpad already had.
    connect(m_contactsPanel, &ContactsPanel::dialRequested, this, [this](const QString &uri) {
        if (m_clientsTargetInput)
            m_clientsTargetInput->setText(uri);
        if (m_callWorkspacePanel)
            m_callWorkspacePanel->placeCall(uri);
    });

    connect(clearTargetBtn, &QPushButton::clicked, this, [this]() {
        if (m_clientsTargetInput)
            m_clientsTargetInput->clear();
    });
    connect(backspaceBtn, &QPushButton::clicked, this, [this]() {
        if (m_clientsTargetInput)
            m_clientsTargetInput->backspace();
    });

    connect(&SipManager::instance(), &SipManager::registrationStateChanged,
            this, [this](RegistrationState state, const QString &, int) {
        if (m_clientsTargetInput)
            m_clientsTargetInput->setEnabled(state == RegistrationState::Registered);
    });

    if (m_clientsTargetInput) {
        connect(m_clientsTargetInput, &QLineEdit::returnPressed, this, [this]() {
            if (m_callWorkspacePanel)
                m_callWorkspacePanel->placeCall(m_clientsTargetInput->text());
        });
    }

    const bool registeredNow = (SipManager::instance().registrationState() == RegistrationState::Registered);
    if (m_clientsTargetInput)
        m_clientsTargetInput->setEnabled(registeredNow);

    m_rttPanel->setRttSession(SipManager::instance().rttSession());
    return page;
}

QWidget *MainWindow::buildCallHistoryPage()
{
    m_callHistoryPanel = new CallHistoryPanel(this);
    connect(m_callHistoryPanel, &CallHistoryPanel::redialRequested,
            this, [this](const QString &uri) {
        if (m_clientsTargetInput)
            m_clientsTargetInput->setText(uri);
        onNavPageRequested(QStringLiteral("clients")); // ensurePage(1) runs synchronously
        if (m_callWorkspacePanel)
            m_callWorkspacePanel->placeCall(uri);
    });
    return m_callHistoryPanel;
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
    const QString buildType = QStringLiteral(APP_BUILD_TYPE_STRING).isEmpty()
        ? tr("Unknown") : QStringLiteral(APP_BUILD_TYPE_STRING);
    QMessageBox::about(
        this,
        tr("About SIP Client"),
        tr("SIP Client - Audio / Video / RTT\n"
           "Desktop Qt client for SIP communication, diagnostics and media testing.\n\n"
           "Version: %1\n"
           "Commit: %2\n"
           "Build: %3")
            .arg(QStringLiteral(APP_VERSION_STRING), QStringLiteral(APP_GIT_COMMIT_HASH), buildType));
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
    connect(m_navRail, &NavRail::pageRequested,        this, &MainWindow::onNavPageRequested);
    connect(m_navRail, &NavRail::importConfigRequested, this, &MainWindow::importConfiguration);
    connect(m_navRail, &NavRail::exportConfigRequested, this, &MainWindow::exportConfiguration);
    connect(m_navRail, &NavRail::aboutRequested,        this, &MainWindow::showAboutDialog);
    connect(m_navRail, &NavRail::helpRequested,         this, &MainWindow::showDiagnosticsInfo);
    connect(m_navRail, &NavRail::exitRequested,         this, &MainWindow::requestApplicationShutdown);
    rootLayout->addWidget(m_navRail);

    m_pageStack = new QStackedWidget(central);
    rootLayout->addWidget(m_pageStack, 1);

    // All pages start as lightweight placeholders — real content is built lazily
    // on first navigation via ensurePage(). This keeps the constructor fast so
    // MainWindow::show() is called before any heavy page construction.
    static const char *const kPageNames[] = {
        "Dashboard", "Clients", "Tools", "Call History", "Settings"
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
        m_toolsPage = new ToolsPage(m_pageStack);
        real = m_toolsPage;
        break;
    case 3: real = buildCallHistoryPage();                       break;
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
    QString toolsSubTab;

    // Old top-level ids for the technical/diagnostic pages that now live as
    // Tools sub-tabs. Kept working so existing deep-link emitters (Dashboard
    // shortcut cards, DiagnosticsCenterPanel's openSipLadderRequested/
    // openLogsRequested) don't need to change what id they emit.
    const bool isToolsSubTabId =
        page == QLatin1String("sipladder") || page == QLatin1String("messaging")
        || page == QLatin1String("presence") || page == QLatin1String("xcap")
        || page == QLatin1String("msrp") || page == QLatin1String("logs")
        || page == QLatin1String("diagnostics");

    if (page == QLatin1String("dashboard")) {
        pageIndex = 0;
    } else if (page == QLatin1String("clients") || page == QLatin1String("accounts")
               || page == QLatin1String("contacts") || page == QLatin1String("dialpad")) {
        pageIndex = 1;
        activePage = QStringLiteral("clients");
    } else if (page == QLatin1String("tools")) {
        pageIndex = 2;
    } else if (isToolsSubTabId) {
        pageIndex = 2;
        toolsSubTab = page;
        activePage = QStringLiteral("tools");
    } else if (page == QLatin1String("callhistory")) {
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
        if (!toolsSubTab.isEmpty() && m_toolsPage)
            m_toolsPage->openSubTab(toolsSubTab);
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

void MainWindow::requestApplicationShutdown()
{
    if (m_shutdownRequested)
        return;
    m_shutdownRequested = true;

    auto &sip = SipManager::instance();
    setEnabled(false);
    saveLayout();

    Logger::instance().info(LogCategory::App, QStringLiteral("Shutdown requested"));

    const auto waitFor = [&sip](auto predicate, int timeoutMs) {
        if (predicate())
            return true;

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        const auto callConn = QObject::connect(&sip, &SipManager::callStateChanged,
                                               &loop, [&]() {
            if (predicate())
                loop.quit();
        });
        const auto regConn = QObject::connect(&sip, &SipManager::registrationStateChanged,
                                              &loop, [&]() {
            if (predicate())
                loop.quit();
        });

        timer.start(timeoutMs);
        loop.exec();

        QObject::disconnect(callConn);
        QObject::disconnect(regConn);
        return predicate();
    };

    const CallState callState = sip.callState();
    if (callState != CallState::Idle && callState != CallState::Failed) {
        Logger::instance().info(LogCategory::App, QStringLiteral("Active call found"));
        Logger::instance().info(LogCategory::App, QStringLiteral("Sending BYE"));
        sip.hangupCall();
        if (waitFor([&sip]() {
                const CallState state = sip.callState();
                return state == CallState::Idle || state == CallState::Failed;
            }, 2000)) {
            Logger::instance().info(LogCategory::App, QStringLiteral("Call terminated"));
        } else {
            Logger::instance().warn(LogCategory::App, QStringLiteral("BYE timeout"));
        }
    }

    if (sip.rttSession() && sip.rttSession()->isActive()) {
        sip.rttSession()->disable();
        Logger::instance().info(LogCategory::App, QStringLiteral("RTT session closed"));
        Logger::instance().info(LogCategory::App, QStringLiteral("LMPE session closed"));
    }

    if (m_clientsVideoPanel)
        m_clientsVideoPanel->stopIdlePreview();
    if (m_videoPanel)
        m_videoPanel->stopIdlePreview();
    Logger::instance().info(LogCategory::App, QStringLiteral("Media stopped"));
    Logger::instance().info(LogCategory::App, QStringLiteral("Camera released"));

    if (sip.registrationState() == RegistrationState::Registered
        || sip.registrationState() == RegistrationState::Registering) {
        Logger::instance().info(LogCategory::App, QStringLiteral("Sending SIP unregister"));
        sip.unregisterActiveProfile();
        if (waitFor([&sip]() {
                return sip.registrationState() == RegistrationState::Unregistered;
            }, 2000)) {
            Logger::instance().info(LogCategory::App, QStringLiteral("Unregister successful"));
        } else {
            Logger::instance().warn(LogCategory::App, QStringLiteral("Unregister timeout"));
        }
    }

    Logger::instance().info(LogCategory::App, QStringLiteral("Stopping SIP stack"));
    sip.shutdown();
    Logger::instance().info(LogCategory::App, QStringLiteral("Application exit"));
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
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
    refreshStatusBarMetrics();
}

void MainWindow::setInitializingStatus(const QString &message)
{
    if (m_statusBar)
        m_statusBar->setSipBackend(message, false);
}

void MainWindow::refreshStatusBarMetrics()
{
    if (!m_statusBar)
        return;

    QString localIp;
    const QString ipSource = localIpSourceText(&localIp);
    m_statusBar->setLocalIp(localIp.isEmpty() ? QStringLiteral("N/A") : localIp,
                            QStringLiteral("Source: %1").arg(ipSource));
}
