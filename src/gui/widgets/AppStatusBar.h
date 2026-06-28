#pragma once
#include <QStatusBar>
#include "sip/SipAccount.h"
#include "media/RtpStats.h"

class QLabel;

class AppStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit AppStatusBar(QWidget *parent = nullptr);

    void setConnectionState(const QString &state);
    void setActiveAccount(const QString &account);
    void setTransport(const QString &transport);
    void setLocalIp(const QString &ip, const QString &tooltip = {});
    void setRtpStats(const RtpStatsSnapshot &stats);
    void setJitter(const QString &jitter, const QString &tooltip = {});
    void setPacketLoss(const QString &loss, const QString &tooltip = {});
    void setRttLatency(const QString &rtt, const QString &tooltip = {});
    void setSipBackend(const QString &name, bool initialized);
    void setRegistrationStatus(RegistrationState state,
                               const QString &statusText,
                               int statusCode);

private:
    QLabel *m_connState{nullptr};
    QLabel *m_account{nullptr};
    QLabel *m_transport{nullptr};
    QLabel *m_localIp{nullptr};
    QLabel *m_jitter{nullptr};
    QLabel *m_loss{nullptr};
    QLabel *m_rttLatency{nullptr};
    QLabel *m_sipBackend{nullptr};
};
