#pragma once

#include <QObject>
#include <QTimer>

#include "sip/SipAccount.h"

// Enforces the SIP registration state machine.
//
// Valid transitions (see isValidTransition()):
//   Unregistered      -> Registering, RegistrationFailed
//   Registering       -> Registered, RegistrationFailed, Unregistering
//   Registered        -> Unregistering, RegistrationFailed (refresh failure)
//   Unregistering     -> Unregistered, RegistrationFailed
//   RegistrationFailed-> Registering, RegistrationFailed, Unregistered
//
// Registering and Unregistering states start a watchdog timer (default 30 s).
// On timeout the machine transitions to RegistrationFailed / Unregistered and
// emits transitionTimedOut before stateChanged.
//
// reset() unconditionally returns to Unregistered regardless of current state
// (used during shutdown).
class RegistrationStateMachine : public QObject
{
    Q_OBJECT
public:
    explicit RegistrationStateMachine(QObject *parent = nullptr);

    RegistrationState state()      const;
    QString           statusText() const;
    int               statusCode() const;

    // Attempt a guarded transition. Returns false and emits transitionRejected
    // when the requested transition is not in the valid table.
    bool tryTransition(RegistrationState to,
                       const QString    &reason,
                       int               statusCode = 0);

    // Force state to Unregistered without table checks (shutdown path).
    void reset(const QString &reason = QStringLiteral("Reset"));

    void setTimeoutMs(int ms);
    int  timeoutMs() const;

signals:
    void stateChanged(RegistrationState newState,
                      const QString    &statusText,
                      int               statusCode);

    void transitionRejected(RegistrationState current,
                            RegistrationState attempted);

    // Emitted just before the forced timeout transition completes.
    void transitionTimedOut(RegistrationState stuckState);

private slots:
    void onTimeout();

private:
    static bool isValidTransition(RegistrationState from, RegistrationState to);
    static bool needsTimeout(RegistrationState state);

    RegistrationState m_state{RegistrationState::Unregistered};
    QString           m_statusText{QStringLiteral("Unregistered")};
    int               m_statusCode{0};
    QTimer            m_timer;
    int               m_timeoutMs{30000};
};
