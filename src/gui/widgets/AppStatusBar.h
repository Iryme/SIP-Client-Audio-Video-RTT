#pragma once
#include <QStatusBar>

class QLabel;

class AppStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit AppStatusBar(QWidget *parent = nullptr);

    void setConnectionState(const QString &state);
    void setActiveAccount(const QString &account);
    void setTransport(const QString &transport);
    void setLocalIp(const QString &ip);
    void setJitter(const QString &jitter);
    void setPacketLoss(const QString &loss);
    void setRttLatency(const QString &rtt);
    void setSipBackend(const QString &name, bool initialized);

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
