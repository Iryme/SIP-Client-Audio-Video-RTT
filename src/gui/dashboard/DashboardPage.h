#pragma once
#include <QWidget>
#include "sip/SipAccount.h"
#include "sip/CallStateMachine.h"

class DashboardHeader;
class DashboardShortcutCard;
class DashboardStatistics;
class DashboardRecentEvents;
class QTimer;

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);

signals:
    void navigateTo(const QString &page);

private slots:
    void onSipStateChanged(RegistrationState state, const QString &text, int code);
    void onCallStateChanged(CallState state, const QString &text, int code);
    void onAudioConnected();
    void onAudioDisconnected();
    void onDevicesChanged();
    void tickUptimeAndMemory();

private:
    void buildShortcutCards(QLayout *layout);
    void populateStats();
    void refreshDeviceStats();

    DashboardHeader       *m_header{nullptr};
    DashboardStatistics   *m_stats{nullptr};
    DashboardRecentEvents *m_events{nullptr};

    DashboardShortcutCard *m_clientsCard{nullptr};
    DashboardShortcutCard *m_mediaCard{nullptr};

    // Counters — track across signal events
    int       m_completedCalls{0};
    int       m_failedCalls{0};
    int       m_totalLogEntries{0};
    int       m_sipEntries{0};
    int       m_rttEntries{0};

    QDateTime m_lastRegTime;
    QDateTime m_callStartTime;
    bool      m_inCall{false};

    QTimer   *m_tickTimer{nullptr};
};
