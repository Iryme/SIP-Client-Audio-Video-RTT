#pragma once
#include <QWidget>
#include "core/Logger.h"
#include "sip/SipAccount.h"
#include "sip/CallStateMachine.h"

class DashboardHeader;
class DashboardShortcutCard;
class DashboardStatistics;
class DashboardRecentEvents;
class QComboBox;
class QLabel;
class QPushButton;
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
    void onSipInitialized();
    void onLogEntryAdded(const LogEntry &entry);
    void tickUptimeAndMemory();
    void refreshQuickSipPanel();
    void refreshCallHistorySummary();

private:
    void buildShortcutCards(QLayout *layout);
    void buildQuickSipActions(QLayout *layout);
    void populateStats();
    void refreshDeviceStats();

    DashboardHeader       *m_header{nullptr};
    DashboardStatistics   *m_stats{nullptr};
    DashboardRecentEvents *m_events{nullptr};

    DashboardShortcutCard *m_clientsCard{nullptr};
    DashboardShortcutCard *m_mediaCard{nullptr};

    // Quick SIP Actions panel
    QComboBox   *m_profileCombo{nullptr};
    QPushButton *m_registerBtn{nullptr};
    QLabel      *m_quickSipStatus{nullptr};

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
