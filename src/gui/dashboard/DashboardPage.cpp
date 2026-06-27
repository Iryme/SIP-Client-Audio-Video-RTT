#include "DashboardPage.h"
#include "DashboardHeader.h"
#include "DashboardShortcutCard.h"
#include "DashboardStatistics.h"
#include "DashboardRecentEvents.h"

#include "core/Logger.h"
#include "core/PerfScope.h"
#include "media/MediaDeviceManager.h"
#include "sip/SipManager.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
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
