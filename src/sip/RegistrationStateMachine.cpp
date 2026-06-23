#include "RegistrationStateMachine.h"

#include "core/Logger.h"

RegistrationStateMachine::RegistrationStateMachine(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout,
            this, &RegistrationStateMachine::onTimeout);
}

RegistrationState RegistrationStateMachine::state()      const { return m_state; }
QString           RegistrationStateMachine::statusText() const { return m_statusText; }
int               RegistrationStateMachine::statusCode() const { return m_statusCode; }
int               RegistrationStateMachine::timeoutMs()  const { return m_timeoutMs; }

void RegistrationStateMachine::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
}

bool RegistrationStateMachine::tryTransition(RegistrationState to,
                                             const QString    &reason,
                                             int               statusCode)
{
    if (!isValidTransition(m_state, to)) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Registration SM: rejected %1 → %2; %3")
                .arg(registrationStateName(m_state),
                     registrationStateName(to),
                     reason));
        emit transitionRejected(m_state, to);
        return false;
    }

    const RegistrationState oldState = m_state;
    m_state      = to;
    m_statusText = reason;
    m_statusCode = statusCode;

    m_timer.stop();
    if (needsTimeout(to))
        m_timer.start(m_timeoutMs);

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Registration SM: %1 → %2; reason=\"%3\"; status=%4")
            .arg(registrationStateName(oldState),
                 registrationStateName(to),
                 reason)
            .arg(statusCode));

    emit stateChanged(to, reason, statusCode);
    return true;
}

void RegistrationStateMachine::reset(const QString &reason)
{
    m_timer.stop();
    const RegistrationState oldState = m_state;
    m_state      = RegistrationState::Unregistered;
    m_statusText = reason;
    m_statusCode = 0;

    if (oldState != RegistrationState::Unregistered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Registration SM: %1 → Unregistered (reset); reason=\"%2\"")
                .arg(registrationStateName(oldState), reason));
    }

    emit stateChanged(RegistrationState::Unregistered, reason, 0);
}

void RegistrationStateMachine::onTimeout()
{
    const RegistrationState stuck = m_state;
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("Registration SM: timeout in state %1 after %2 ms")
            .arg(registrationStateName(stuck))
            .arg(m_timeoutMs));

    emit transitionTimedOut(stuck);

    const RegistrationState fallback =
        (stuck == RegistrationState::Unregistering)
            ? RegistrationState::Unregistered
            : RegistrationState::RegistrationFailed;

    const RegistrationState oldState = m_state;
    m_state      = fallback;
    m_statusText = QStringLiteral("Registration timed out");
    m_statusCode = 0;

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Registration SM: %1 → %2 (timeout forced)")
            .arg(registrationStateName(oldState),
                 registrationStateName(fallback)));

    emit stateChanged(fallback, m_statusText, 0);
}

bool RegistrationStateMachine::isValidTransition(RegistrationState from,
                                                 RegistrationState to)
{
    using RS = RegistrationState;
    switch (from) {
    case RS::Unregistered:
        return to == RS::Registering
            || to == RS::RegistrationFailed;          // preflight failure
    case RS::Registering:
        return to == RS::Registered
            || to == RS::RegistrationFailed
            || to == RS::Unregistering;               // cancel in-flight
    case RS::Registered:
        return to == RS::Unregistering
            || to == RS::RegistrationFailed;          // registration refresh failure
    case RS::Unregistering:
        return to == RS::Unregistered
            || to == RS::RegistrationFailed;
    case RS::RegistrationFailed:
        return to == RS::Registering                  // retry
            || to == RS::RegistrationFailed           // retry preflight failure
            || to == RS::Unregistered;                // cleanup after failure
    }
    return false;
}

bool RegistrationStateMachine::needsTimeout(RegistrationState state)
{
    return state == RegistrationState::Registering
        || state == RegistrationState::Unregistering;
}
