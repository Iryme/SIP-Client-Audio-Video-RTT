#pragma once
#include <QWidget>
#include "sip/SipAccount.h"
#include "sip/CallStateMachine.h"

class QLabel;

class DashboardHeader : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardHeader(QWidget *parent = nullptr);

    void setSipStatus(RegistrationState state);
    void setMediaStatus(bool connected);
    void setCallStatus(CallState state);

private slots:
    void updateClock();

private:
    static QWidget *makeStatusBlock(const QString &category, QWidget *parent,
                                    QLabel **dotOut, QLabel **statusTextOut);
    static void applyDot(QLabel *dot, QLabel *text,
                         const QString &color, const QString &label);

    QLabel *m_clockLabel{nullptr};
    QLabel *m_sipDot{nullptr};
    QLabel *m_sipText{nullptr};
    QLabel *m_mediaDot{nullptr};
    QLabel *m_mediaText{nullptr};
    QLabel *m_callDot{nullptr};
    QLabel *m_callText{nullptr};
};
