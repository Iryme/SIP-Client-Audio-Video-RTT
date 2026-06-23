#pragma once

#include <QObject>
#include <QTimer>

enum class CallState {
    Idle,
    OutgoingInit,
    Ringing,
    IncomingRinging,
    Connecting,
    Active,
    Held,
    Disconnecting,
    Failed
};

Q_DECLARE_METATYPE(CallState)

QString callStateName(CallState state);
QString callStateDisplayText(CallState state);

// Enforces the SIP call state machine.
//
// Valid transitions:
//   Idle            -> OutgoingInit, IncomingRinging
//   OutgoingInit    -> Ringing, Connecting, Failed, Disconnecting
//   Ringing         -> Connecting, Active, Failed, Disconnecting
//   IncomingRinging -> Connecting, Active, Idle, Disconnecting
//   Connecting      -> Active, Failed
//   Active          -> Held, Disconnecting, Failed
//   Held            -> Active, Disconnecting
//   Disconnecting   -> Idle, Failed
//   Failed          -> Idle
//
// OutgoingInit and Disconnecting states start a watchdog timer (default 30 s).
// On timeout: OutgoingInit -> Failed, Disconnecting -> Idle.
// reset() unconditionally returns to Idle (used during shutdown / call cleanup).
class CallStateMachine : public QObject
{
    Q_OBJECT
public:
    explicit CallStateMachine(QObject *parent = nullptr);

    CallState state()      const;
    QString   statusText() const;
    int       statusCode() const;

    bool tryTransition(CallState to, const QString &reason, int statusCode = 0);
    void reset(const QString &reason = QStringLiteral("Reset"));

    void setTimeoutMs(int ms);
    int  timeoutMs() const;

signals:
    void stateChanged(CallState newState, const QString &statusText, int statusCode);
    void transitionRejected(CallState current, CallState attempted);
    void transitionTimedOut(CallState stuckState);

private slots:
    void onTimeout();

private:
    static bool isValidTransition(CallState from, CallState to);
    static bool needsTimeout(CallState state);

    CallState m_state{CallState::Idle};
    QString   m_statusText{QStringLiteral("Idle")};
    int       m_statusCode{0};
    QTimer    m_timer;
    int       m_timeoutMs{30000};
};
