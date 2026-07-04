#include "DashboardPage.h"
#include "DashboardHeader.h"
#include "DashboardShortcutCard.h"
#include "DashboardStatistics.h"
#include "DashboardRecentEvents.h"

#include "core/CallHistoryStore.h"
#include "core/Logger.h"
#include "core/PerfScope.h"
#include "media/MediaDeviceManager.h"
#include "sip/SipManager.h"
#include "sip/SipProfile.h"
#include "sip/SipProfileManager.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <psapi.h>
#endif

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DashboardPage");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ──────────────────────────────────────────────────────────────
    m_header = new DashboardHeader(this);
    root->addWidget(m_header);

    // ── Shortcut cards ──────────────────────────────────────────────────────
    auto *cardsBar = new QWidget(this);
    cardsBar->setObjectName("CardsBar");
    cardsBar->setStyleSheet(
        "QWidget#CardsBar { background-color: #111828; border-bottom: 1px solid #1a2438; }");
    auto *cardsLayout = new QHBoxLayout(cardsBar);
    cardsLayout->setContentsMargins(20, 14, 20, 14);
    cardsLayout->setSpacing(12);
    buildShortcutCards(cardsLayout);
    root->addWidget(cardsBar);

    // ── Quick SIP Actions ────────────────────────────────────────────────────
    auto *quickSipBar = new QWidget(this);
    quickSipBar->setObjectName("QuickSipBar");
    quickSipBar->setStyleSheet(
        "QWidget#QuickSipBar { background-color: #0f1624; border-bottom: 1px solid #1a2438; }");
    auto *quickSipLayout = new QHBoxLayout(quickSipBar);
    quickSipLayout->setContentsMargins(20, 10, 20, 10);
    quickSipLayout->setSpacing(12);
    buildQuickSipActions(quickSipLayout);
    root->addWidget(quickSipBar);

    // ── Bottom: statistics + recent events ──────────────────────────────────
    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(0);

    auto *statsFrame = new QFrame(this);
    statsFrame->setObjectName("StatsFrame");
    statsFrame->setStyleSheet(
        "QFrame#StatsFrame { background-color: #111624; border-right: 1px solid #1a2438; }");
    auto *statsInner = new QVBoxLayout(statsFrame);
    statsInner->setContentsMargins(0, 0, 0, 0);
    m_stats = new DashboardStatistics(statsFrame);
    statsInner->addWidget(m_stats);
    bottomRow->addWidget(statsFrame, 3);

    auto *eventsFrame = new QFrame(this);
    eventsFrame->setObjectName("EventsFrame");
    eventsFrame->setStyleSheet("QFrame#EventsFrame { background-color: #0f1420; }");
    auto *eventsInner = new QVBoxLayout(eventsFrame);
    eventsInner->setContentsMargins(0, 0, 0, 0);
    m_events = new DashboardRecentEvents(eventsFrame);
    eventsInner->addWidget(m_events);
    bottomRow->addWidget(eventsFrame, 2);

    root->addLayout(bottomRow, 1);

    // ── Statistics definitions ────────────────────────────────────────────
    populateStats();

    // ── Signal connections ───────────────────────────────────────────────
    // Named-slot connections use Qt::UniqueConnection as a safety net.
    // Lambda connections cannot use Qt::UniqueConnection (Qt asserts) — they
    // are safe without it because DashboardPage is constructed exactly once
    // (guarded by MainWindow::m_pageBuilt[0]).
    auto &sip = SipManager::instance();
    connect(&sip, &SipManager::registrationStateChanged,
            this, &DashboardPage::onSipStateChanged,
            Qt::UniqueConnection);
    connect(&sip, &SipManager::callStateChanged,
            this, &DashboardPage::onCallStateChanged,
            Qt::UniqueConnection);
    connect(&sip, &SipManager::audioMediaConnected,
            this, &DashboardPage::onAudioConnected,
            Qt::UniqueConnection);
    connect(&sip, &SipManager::audioMediaDisconnected,
            this, &DashboardPage::onAudioDisconnected,
            Qt::UniqueConnection);
    connect(&sip, &SipManager::initialized,
            this, &DashboardPage::onSipInitialized,
            Qt::UniqueConnection);

    connect(&MediaDeviceManager::instance(), &MediaDeviceManager::devicesChanged,
            this, &DashboardPage::onDevicesChanged,
            Qt::UniqueConnection);

    connect(&Logger::instance(), &Logger::entryAdded,
            this, &DashboardPage::onLogEntryAdded,
            Qt::UniqueConnection);

    connect(&CallHistoryStore::instance(), &CallHistoryStore::historyChanged,
            this, &DashboardPage::refreshCallHistorySummary,
            Qt::UniqueConnection);

    auto &pm = SipProfileManager::instance();
    connect(&pm, &SipProfileManager::profileAdded,         this, [this](const QString &) { refreshQuickSipPanel(); });
    connect(&pm, &SipProfileManager::profileUpdated,        this, [this](const QString &) { refreshQuickSipPanel(); });
    connect(&pm, &SipProfileManager::profileRemoved,        this, [this](const QString &) { refreshQuickSipPanel(); });
    connect(&pm, &SipProfileManager::activeProfileChanged,  this, [this](const QString &) { refreshQuickSipPanel(); });

    // ── Periodic tick: uptime, call duration, memory ─────────────────────
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(5000);
    connect(m_tickTimer, &QTimer::timeout, this, &DashboardPage::tickUptimeAndMemory);
    m_tickTimer->start();

    // ── Initial state ────────────────────────────────────────────────────
    onSipStateChanged(sip.registrationState(), sip.registrationStatusText(),
                      sip.registrationStatusCode());
    onCallStateChanged(sip.callState(), sip.callStatusText(), 0);
    m_header->setMediaStatus(false);
    refreshDeviceStats();
    tickUptimeAndMemory();
    refreshQuickSipPanel();
    refreshCallHistorySummary();
}

void DashboardPage::buildShortcutCards(QLayout *layout)
{
    auto *hlay = qobject_cast<QHBoxLayout *>(layout);
    if (!hlay) return;

    const auto &sip = SipManager::instance();
    const QString callStatus = sip.callState() != CallState::Idle
        ? tr("In Call") : tr("No active call");

    const int totalDevices = MediaDeviceManager::instance().listCameras().size()
                           + MediaDeviceManager::instance().listMicrophones().size()
                           + MediaDeviceManager::instance().listSpeakers().size();

    m_clientsCard = new DashboardShortcutCard(
        QStringLiteral("\U0001F4DE"), tr("Clients"),
        tr("Video, call control & RTT"),
        QStringLiteral("clients"), this);
    m_clientsCard->setStatus(callStatus);

    auto *ladderCard = new DashboardShortcutCard(
        QStringLiteral("\U0001F4C8"), tr("SIP Ladder"),
        tr("SIP trace & message diagram"),
        QStringLiteral("sipladder"), this);
    ladderCard->setStatus(tr("Tap to open"));

    auto *logsCard = new DashboardShortcutCard(
        QStringLiteral("\U0001F4C4"), tr("Logs"),
        tr("Diagnostics & log viewer"),
        QStringLiteral("logs"), this);
    logsCard->setStatus(tr("Tap to open"));

    m_mediaCard = new DashboardShortcutCard(
        QStringLiteral("\U0001F3A4"), tr("Video"),
        tr("Camera preview & devices"),
        QStringLiteral("settings-video"), this);
    m_mediaCard->setStatus(
        totalDevices > 0
            ? tr("%1 device(s) detected").arg(totalDevices)
            : tr("No devices"));

    auto *settingsCard = new DashboardShortcutCard(
        QStringLiteral("⚙"), tr("Settings"),
        tr("SIP profiles & configuration"),
        QStringLiteral("settings"), this);
    settingsCard->setStatus(tr("Tap to open"));

    for (auto *card : {m_clientsCard, ladderCard, logsCard, m_mediaCard, settingsCard}) {
        hlay->addWidget(card, 1);
        connect(card, &DashboardShortcutCard::navigateTo,
                this, &DashboardPage::navigateTo);
    }
}

void DashboardPage::buildQuickSipActions(QLayout *layout)
{
    auto *hlay = qobject_cast<QHBoxLayout *>(layout);
    if (!hlay) return;

    // Section label
    auto *sectionLabel = new QLabel(tr("Quick SIP Actions"), this);
    sectionLabel->setStyleSheet("color: #6a7a9a; font-size: 10px; font-weight: bold; letter-spacing: 1px;");
    hlay->addWidget(sectionLabel);

    // Profile dropdown
    auto *profileLabel = new QLabel(tr("Profile:"), this);
    profileLabel->setStyleSheet("color: #aabbcc; font-size: 11px;");
    hlay->addWidget(profileLabel);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setObjectName("QuickSipProfileCombo");
    m_profileCombo->setMinimumWidth(180);
    m_profileCombo->setFixedHeight(28);
    m_profileCombo->setStyleSheet(
        "QComboBox { background: #1a2438; color: #cce0ff; border: 1px solid #2a3a58;"
        " border-radius: 4px; padding: 2px 8px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2438; color: #cce0ff; border: 1px solid #2a3a58; }");
    hlay->addWidget(m_profileCombo);

    // Register / Unregister button
    m_registerBtn = new QPushButton(tr("Register"), this);
    m_registerBtn->setObjectName("QuickRegisterBtn");
    m_registerBtn->setFixedHeight(28);
    m_registerBtn->setMinimumWidth(100);
    m_registerBtn->setStyleSheet(
        "QPushButton#QuickRegisterBtn { background: #1e3a1e; color: #50c878; border: 1px solid #2a5a2a;"
        " border-radius: 4px; font-size: 11px; font-weight: bold; padding: 0 12px; }"
        "QPushButton#QuickRegisterBtn:hover { background: #2a4a2a; }"
        "QPushButton#QuickRegisterBtn:disabled { background: #151e15; color: #3a5a3a; border-color: #1a2a1a; }");
    hlay->addWidget(m_registerBtn);

    // Status label
    m_quickSipStatus = new QLabel(tr("—"), this);
    m_quickSipStatus->setObjectName("QuickSipStatus");
    m_quickSipStatus->setStyleSheet("color: #888; font-size: 10px;");
    m_quickSipStatus->setMinimumWidth(200);
    hlay->addWidget(m_quickSipStatus, 1);

    // Profile selection changes active profile (no auto-register)
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        const QString profileId = m_profileCombo->itemData(idx).toString();
        if (!profileId.isEmpty() && profileId != SipProfileManager::instance().activeProfileId()) {
            SipProfileManager::instance().setActiveProfileId(profileId);
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Dashboard: active profile changed to %1").arg(profileId));
        }
        refreshQuickSipPanel();
    });

    // Register / Unregister button action
    connect(m_registerBtn, &QPushButton::clicked, this, [this]() {
        const RegistrationState st = SipManager::instance().registrationState();
        if (st == RegistrationState::Registered || st == RegistrationState::Registering) {
            Logger::instance().info(LogCategory::Sip, QStringLiteral("Dashboard: unregister requested"));
            SipManager::instance().unregisterActiveProfile();
        } else {
            // Select the combo profile as active before registering
            const QString profileId = m_profileCombo->currentData().toString();
            if (!profileId.isEmpty())
                SipProfileManager::instance().setActiveProfileId(profileId);
            Logger::instance().info(LogCategory::Sip, QStringLiteral("Dashboard: register requested"));
            SipManager::instance().registerActiveProfile();
        }
    });
}

void DashboardPage::refreshCallHistorySummary()
{
    if (!m_stats)
        return;

    const auto &store = CallHistoryStore::instance();
    m_stats->setValue(QStringLiteral("callsToday"),  QString::number(store.callsToday()));
    m_stats->setValue(QStringLiteral("missedToday"), QString::number(store.missedToday()));

    const CallHistoryEntry last = store.lastCall();
    if (last.isNull()) {
        m_stats->setValue(QStringLiteral("lastCall"), tr("—"));
    } else {
        const QString who = last.displayName.isEmpty() ? last.remoteUri : last.displayName;
        m_stats->setValue(QStringLiteral("lastCall"),
            QStringLiteral("%1 (%2)").arg(who, callResultName(last.result)));
    }
}

void DashboardPage::refreshQuickSipPanel()
{
    if (!m_profileCombo || !m_registerBtn || !m_quickSipStatus)
        return;

    const auto &pm     = SipProfileManager::instance();
    const auto profiles = pm.profiles();
    const QString activeId = pm.activeProfileId();

    // Rebuild combo if profile list changed
    {
        QSignalBlocker b(m_profileCombo);
        m_profileCombo->clear();
        if (profiles.isEmpty()) {
            m_profileCombo->addItem(tr("No SIP profiles configured"), QString{});
            m_profileCombo->setEnabled(false);
        } else {
            m_profileCombo->setEnabled(true);
            for (const SipProfile &p : profiles) {
                const QString label = p.displayName.isEmpty()
                    ? p.effectiveSipUri()
                    : QStringLiteral("%1 (%2)").arg(p.displayName, p.effectiveSipUri());
                m_profileCombo->addItem(label, p.profileId);
            }
            // Select active profile in combo
            for (int i = 0; i < m_profileCombo->count(); ++i) {
                if (m_profileCombo->itemData(i).toString() == activeId) {
                    m_profileCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    // Button state
    const RegistrationState regState = SipManager::instance().registrationState();
    const bool hasProfiles = !profiles.isEmpty();
    switch (regState) {
    case RegistrationState::Registered:
        m_registerBtn->setText(tr("Unregister"));
        m_registerBtn->setEnabled(true);
        m_registerBtn->setStyleSheet(
            "QPushButton#QuickRegisterBtn { background: #2a1e1e; color: #e07070; border: 1px solid #5a2a2a;"
            " border-radius: 4px; font-size: 11px; font-weight: bold; padding: 0 12px; }"
            "QPushButton#QuickRegisterBtn:hover { background: #3a2020; }");
        break;
    case RegistrationState::Registering:
        m_registerBtn->setText(tr("Registering…"));
        m_registerBtn->setEnabled(false);
        m_registerBtn->setStyleSheet(
            "QPushButton#QuickRegisterBtn { background: #1a2020; color: #e0b850; border: 1px solid #3a3a20;"
            " border-radius: 4px; font-size: 11px; font-weight: bold; padding: 0 12px; }");
        break;
    case RegistrationState::Unregistering:
        m_registerBtn->setText(tr("Unregistering…"));
        m_registerBtn->setEnabled(false);
        m_registerBtn->setStyleSheet(
            "QPushButton#QuickRegisterBtn { background: #1a2020; color: #e0b850; border: 1px solid #3a3a20;"
            " border-radius: 4px; font-size: 11px; font-weight: bold; padding: 0 12px; }");
        break;
    default: // Unregistered / RegistrationFailed
        m_registerBtn->setText(tr("Register"));
        m_registerBtn->setEnabled(hasProfiles);
        m_registerBtn->setStyleSheet(
            "QPushButton#QuickRegisterBtn { background: #1e3a1e; color: #50c878; border: 1px solid #2a5a2a;"
            " border-radius: 4px; font-size: 11px; font-weight: bold; padding: 0 12px; }"
            "QPushButton#QuickRegisterBtn:hover { background: #2a4a2a; }"
            "QPushButton#QuickRegisterBtn:disabled { background: #151e15; color: #3a5a3a; border-color: #1a2a1a; }");
        break;
    }

    // Status text
    const SipProfile activeProf = pm.hasProfile(activeId) ? pm.profile(activeId) : SipProfile{};
    if (activeProf.isNull()) {
        m_quickSipStatus->setText(tr("No profile selected"));
        m_quickSipStatus->setStyleSheet("color: #888; font-size: 10px;");
    } else {
        QString regStateText;
        QString color;
        switch (regState) {
        case RegistrationState::Registered:
            regStateText = tr("registered"); color = QStringLiteral("#50c878"); break;
        case RegistrationState::Registering:
            regStateText = tr("pending"); color = QStringLiteral("#e0b850"); break;
        case RegistrationState::Unregistering:
            regStateText = tr("unregistering"); color = QStringLiteral("#e0b850"); break;
        case RegistrationState::RegistrationFailed:
            regStateText = tr("error"); color = QStringLiteral("#e05050"); break;
        default:
            regStateText = tr("unregistered"); color = QStringLiteral("#888888"); break;
        }
        const QString registrar = activeProf.registrar.isEmpty() ? activeProf.sipDomain : activeProf.registrar;
        m_quickSipStatus->setText(
            QStringLiteral("<span style='color:%1'>%2</span>"
                           " &nbsp;|&nbsp; <span style='color:#aaa'>%3</span>"
                           " &nbsp;|&nbsp; <span style='color:#7a9abc'>%4</span>")
                .arg(color, regStateText, activeProf.effectiveSipUri(), registrar));
        m_quickSipStatus->setStyleSheet("font-size: 10px;");
    }
}

void DashboardPage::populateStats()
{
    m_stats->addSection(tr("SIP"));
    m_stats->addStat(QStringLiteral("registeredAccounts"), tr("Registered Accounts"));
    m_stats->addStat(QStringLiteral("activeCalls"),        tr("Active Calls"));
    m_stats->addStat(QStringLiteral("completedCalls"),     tr("Completed Calls"));
    m_stats->addStat(QStringLiteral("failedCalls"),        tr("Failed Calls"));
    m_stats->addStat(QStringLiteral("lastRegistration"),   tr("Last Registration"));

    m_stats->addSection(tr("MESSAGES"));
    m_stats->addStat(QStringLiteral("logMessages"),   tr("Messages Logged"));
    m_stats->addStat(QStringLiteral("sipMessages"),   tr("SIP Messages"));
    m_stats->addStat(QStringLiteral("rttMessages"),   tr("RTT Messages"));
    m_stats->addStat(QStringLiteral("packetCapture"), tr("Packet Capture"));

    m_stats->addSection(tr("MEDIA"));
    m_stats->addStat(QStringLiteral("selectedCamera"),   tr("Selected Camera"));
    m_stats->addStat(QStringLiteral("selectedMic"),      tr("Selected Microphone"));
    m_stats->addStat(QStringLiteral("selectedSpeaker"),  tr("Selected Speaker"));

    m_stats->addSection(tr("SYSTEM"));
    m_stats->addStat(QStringLiteral("appUptime"),    tr("Application Uptime"));
    m_stats->addStat(QStringLiteral("callDuration"), tr("Call Duration"));
    m_stats->addStat(QStringLiteral("memoryUsage"),  tr("Memory Usage"));
    m_stats->addStat(QStringLiteral("cpuUsage"),     tr("CPU Usage"));

    m_stats->addSection(tr("CALL HISTORY"));
    m_stats->addStat(QStringLiteral("callsToday"),  tr("Calls Today"));
    m_stats->addStat(QStringLiteral("missedToday"), tr("Missed Today"));
    m_stats->addStat(QStringLiteral("lastCall"),    tr("Last Call"));

    // Known-unavailable entries
    m_stats->setValue(QStringLiteral("registeredAccounts"), QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("activeCalls"),        QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("completedCalls"),     QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("failedCalls"),        QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("logMessages"),        QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("sipMessages"),        QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("rttMessages"),        QStringLiteral("0"));
    m_stats->setValue(QStringLiteral("packetCapture"),      QStringLiteral("—"));
    m_stats->setValue(QStringLiteral("cpuUsage"),           QStringLiteral("—"));
#ifndef Q_OS_WIN
    m_stats->setValue(QStringLiteral("memoryUsage"),        QStringLiteral("—"));
#endif
}

void DashboardPage::refreshDeviceStats()
{
    auto &mgr = MediaDeviceManager::instance();

    const auto cameras = mgr.listCameras();
    const auto mics    = mgr.listMicrophones();
    const auto spks    = mgr.listSpeakers();

    auto firstName = [](const QList<MediaDevice> &list) -> QString {
        if (list.isEmpty()) return QStringLiteral("—");
        const auto it = std::find_if(list.begin(), list.end(),
                                     [](const MediaDevice &d) { return d.isDefault; });
        return it != list.end() ? it->displayName : list.first().displayName;
    };

    m_stats->setValue(QStringLiteral("selectedCamera"),  firstName(cameras));
    m_stats->setValue(QStringLiteral("selectedMic"),     firstName(mics));
    m_stats->setValue(QStringLiteral("selectedSpeaker"), firstName(spks));

    if (m_mediaCard) {
        const int total = cameras.size() + mics.size() + spks.size();
        m_mediaCard->setStatus(
            total > 0
                ? tr("%1 device(s) detected").arg(total)
                : tr("No devices"));
    }
}

void DashboardPage::onSipStateChanged(RegistrationState state, const QString &, int)
{
    m_header->setSipStatus(state);
    refreshQuickSipPanel();

    const bool reg = state == RegistrationState::Registered;
    m_stats->setValue(QStringLiteral("registeredAccounts"), reg ? QStringLiteral("1") : QStringLiteral("0"));

    if (reg) {
        m_lastRegTime = QDateTime::currentDateTime();
        m_stats->setValue(QStringLiteral("lastRegistration"),
                          m_lastRegTime.toString(QStringLiteral("dd MMM hh:mm:ss")));
    }
}

void DashboardPage::onCallStateChanged(CallState state, const QString &, int)
{
    m_header->setCallStatus(state);

    const bool idle = (state == CallState::Idle);
    m_stats->setValue(QStringLiteral("activeCalls"), idle ? QStringLiteral("0") : QStringLiteral("1"));

    if (state == CallState::Active && !m_inCall) {
        m_inCall       = true;
        m_callStartTime = QDateTime::currentDateTime();
    }

    if (state == CallState::Idle && m_inCall) {
        m_inCall = false;
        ++m_completedCalls;
        m_stats->setValue(QStringLiteral("completedCalls"), QString::number(m_completedCalls));
        m_stats->setValue(QStringLiteral("callDuration"),   QStringLiteral("—"));
    }

    if (state == CallState::Failed) {
        if (m_inCall) m_inCall = false;
        m_stats->setValue(QStringLiteral("activeCalls"), QStringLiteral("0"));
        ++m_failedCalls;
        m_stats->setValue(QStringLiteral("failedCalls"), QString::number(m_failedCalls));
    }

    if (m_clientsCard) {
        m_clientsCard->setStatus(
            idle ? tr("No active call") : tr("In Call"),
            idle ? QStringLiteral("#6a7a9a") : QStringLiteral("#4caf50"));
    }
}

void DashboardPage::onAudioConnected()
{
    m_header->setMediaStatus(true);
}

void DashboardPage::onAudioDisconnected()
{
    m_header->setMediaStatus(false);
}

void DashboardPage::onDevicesChanged()
{
    refreshDeviceStats();
}

void DashboardPage::onSipInitialized()
{
    const RegistrationState state = SipManager::instance().registrationState();
    m_header->setSipStatus(state);
    m_stats->setValue(QStringLiteral("registeredAccounts"),
        state == RegistrationState::Registered ? QStringLiteral("1") : QStringLiteral("0"));
}

void DashboardPage::onLogEntryAdded(const LogEntry &entry)
{
    ++m_totalLogEntries;
    if (entry.category == LogCategory::Sip) ++m_sipEntries;
    if (entry.category == LogCategory::Rtt) ++m_rttEntries;
    m_stats->setValue(QStringLiteral("logMessages"), QString::number(m_totalLogEntries));
    m_stats->setValue(QStringLiteral("sipMessages"),  QString::number(m_sipEntries));
    m_stats->setValue(QStringLiteral("rttMessages"),  QString::number(m_rttEntries));
}

void DashboardPage::tickUptimeAndMemory()
{
    // Uptime
    const qint64 ms = PerfScope::msecsSinceAppStart();
    if (ms >= 0) {
        const int total = static_cast<int>(ms / 1000);
        const int h = total / 3600;
        const int m = (total % 3600) / 60;
        const int s = total % 60;
        m_stats->setValue(QStringLiteral("appUptime"),
            QStringLiteral("%1h %2m %3s")
                .arg(h)
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0')));
    }

    // Call duration
    if (m_inCall && m_callStartTime.isValid()) {
        const int elapsed = static_cast<int>(m_callStartTime.secsTo(QDateTime::currentDateTime()));
        const int h = elapsed / 3600;
        const int m = (elapsed % 3600) / 60;
        const int s = elapsed % 60;
        m_stats->setValue(QStringLiteral("callDuration"),
            h > 0
                ? QStringLiteral("%1:%2:%3")
                      .arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'))
                : QStringLiteral("%1:%2")
                      .arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')));
    }

    // Memory (Windows)
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        const qint64 mb = static_cast<qint64>(pmc.WorkingSetSize) / (1024 * 1024);
        m_stats->setValue(QStringLiteral("memoryUsage"),
                          QStringLiteral("%1 MB").arg(mb));
    }
#endif
}
