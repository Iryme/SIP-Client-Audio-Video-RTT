#include "CallStateMachine.h"

#include "core/Logger.h"

QString callStateName(CallState state)
{
    switch (state) {
    case CallState::Idle:            return QStringLiteral("Idle");
    case CallState::OutgoingInit:    return QStringLiteral("OutgoingInit");
    case CallState::Ringing:         return QStringLiteral("Ringing");
    case CallState::IncomingRinging: return QStringLiteral("IncomingRinging");
    case CallState::Connecting:      return QStringLiteral("Connecting");
    case CallState::Active:          return QStringLiteral("Active");
    case CallState::Held:            return QStringLiteral("Held");
    case CallState::Disconnecting:   return QStringLiteral("Disconnecting");
    case CallState::Failed:          return QStringLiteral("Failed");
    }
    return QStringLiteral("Idle");
}

QString callStateDisplayText(CallState state)
{
    switch (state) {
    case CallState::Idle:            return QStringLiteral("Idle");
    case CallState::OutgoingInit:    return QStringLiteral("Calling...");
    case CallState::Ringing:         return QStringLiteral("Ringing...");
    case CallState::IncomingRinging: return QStringLiteral("Incoming Call");
    case CallState::Connecting:      return QStringLiteral("Connecting...");
    case CallState::Active:          return QStringLiteral("Connected");
    case CallState::Held:            return QStringLiteral("Paused");
    case CallState::Disconnecting:   return QStringLiteral("Ending...");
    case CallState::Failed:          return QStringLiteral("Call Failed");
    }
    return QStringLiteral("Idle");
}

CallStateMachine::CallStateMachine(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &CallStateMachine::onTimeout);
}

CallState CallStateMachine::state()      const { return m_state; }
QString   CallStateMachine::statusText() const { return m_statusText; }
int       CallStateMachine::statusCode() const { return m_statusCode; }
int       CallStateMachine::timeoutMs()  const { return m_timeoutMs; }

void CallStateMachine::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
}

bool CallStateMachine::tryTransition(CallState to, const QString &reason, int statusCode)
{
    if (!isValidTransition(m_state, to)) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Call SM: rejected %1 → %2; %3")
                .arg(callStateName(m_state), callStateName(to), reason));
        emit transitionRejected(m_state, to);
        return false;
    }

    const CallState oldState = m_state;
    m_state      = to;
    m_statusText = reason;
    m_statusCode = statusCode;

    m_timer.stop();
    if (needsTimeout(to))
        m_timer.start(m_timeoutMs);

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Call SM: %1 → %2; reason=\"%3\"; status=%4")
            .arg(callStateName(oldState), callStateName(to), reason)
            .arg(statusCode));

    emit stateChanged(to, reason, statusCode);
    return true;
}

void CallStateMachine::reset(const QString &reason)
{
    m_timer.stop();
    const CallState oldState = m_state;
    m_state      = CallState::Idle;
    m_statusText = reason;
    m_statusCode = 0;

    if (oldState != CallState::Idle) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Call SM: %1 → Idle (reset); reason=\"%2\"")
                .arg(callStateName(oldState), reason));
    }

    emit stateChanged(CallState::Idle, reason, 0);
}

void CallStateMachine::onTimeout()
{
    const CallState stuck = m_state;
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("Call SM: timeout in state %1 after %2 ms")
            .arg(callStateName(stuck))
            .arg(m_timeoutMs));

    emit transitionTimedOut(stuck);

    const CallState fallback =
        (stuck == CallState::Disconnecting) ? CallState::Idle : CallState::Failed;

    const CallState oldState = m_state;
    m_state      = fallback;
    m_statusText = QStringLiteral("Call timed out");
    m_statusCode = 0;

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Call SM: %1 → %2 (timeout forced)")
            .arg(callStateName(oldState), callStateName(fallback)));

    emit stateChanged(fallback, m_statusText, 0);
}

bool CallStateMachine::isValidTransition(CallState from, CallState to)
{
    using CS = CallState;
    switch (from) {
    case CS::Idle:
        return to == CS::OutgoingInit
            || to == CS::IncomingRinging;
    case CS::OutgoingInit:
        return to == CS::Ringing
            || to == CS::Connecting
            || to == CS::Failed
            || to == CS::Disconnecting;
    case CS::Ringing:
        return to == CS::Connecting
            || to == CS::Active
            || to == CS::Failed
            || to == CS::Disconnecting;
    case CS::IncomingRinging:
        return to == CS::Connecting
            || to == CS::Active
            || to == CS::Idle           // rejected by local user
            || to == CS::Disconnecting  // caller cancelled
            || to == CS::Failed;        // session terminated before answer
    case CS::Connecting:
        return to == CS::Active
            || to == CS::Failed;
    case CS::Active:
        return to == CS::Held
            || to == CS::Disconnecting
            || to == CS::Failed;
    case CS::Held:
        return to == CS::Active
            || to == CS::Disconnecting;
    case CS::Disconnecting:
        return to == CS::Idle
            || to == CS::Failed;
    case CS::Failed:
        return to == CS::Idle;
    }
    return false;
}

bool CallStateMachine::needsTimeout(CallState state)
{
    return state == CallState::OutgoingInit
        || state == CallState::Disconnecting;
}
