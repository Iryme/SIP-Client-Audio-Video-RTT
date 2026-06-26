#include "emergency/EmergencyCallStateMachine.h"

QString emergencyCallStateName(EmergencyCallState state)
{
    switch (state) {
    case EmergencyCallState::Idle:            return QStringLiteral("Idle");
    case EmergencyCallState::Preparing:       return QStringLiteral("Preparing");
    case EmergencyCallState::LocationPending: return QStringLiteral("LocationPending");
    case EmergencyCallState::ReadyToDial:     return QStringLiteral("ReadyToDial");
    case EmergencyCallState::Dialing:         return QStringLiteral("Dialing");
    case EmergencyCallState::Active:          return QStringLiteral("Active");
    case EmergencyCallState::Failed:          return QStringLiteral("Failed");
    case EmergencyCallState::Ended:           return QStringLiteral("Ended");
    }
    return QStringLiteral("Unknown");
}

EmergencyCallStateMachine::EmergencyCallStateMachine(QObject *parent)
    : QObject(parent)
{
}

EmergencyCallState EmergencyCallStateMachine::state() const
{
    return m_state;
}

QString EmergencyCallStateMachine::stateName() const
{
    return emergencyCallStateName(m_state);
}

bool EmergencyCallStateMachine::canTransitionTo(EmergencyCallState to) const
{
    using S = EmergencyCallState;
    switch (m_state) {
    case S::Idle:
        return to == S::Preparing;
    case S::Preparing:
        return to == S::LocationPending || to == S::ReadyToDial || to == S::Failed;
    case S::LocationPending:
        return to == S::ReadyToDial || to == S::Failed;
    case S::ReadyToDial:
        return to == S::Dialing || to == S::Failed;
    case S::Dialing:
        return to == S::Active || to == S::Failed;
    case S::Active:
        return to == S::Ended || to == S::Failed;
    case S::Failed:
    case S::Ended:
        return to == S::Idle;
    }
    return false;
}

bool EmergencyCallStateMachine::transition(EmergencyCallState to, const QString &reason)
{
    if (!canTransitionTo(to)) {
        emit transitionRejected(to, m_state);
        return false;
    }
    const EmergencyCallState from = m_state;
    m_state = to;
    emit stateChanged(m_state, from, reason);
    return true;
}

void EmergencyCallStateMachine::reset()
{
    if (m_state == EmergencyCallState::Idle)
        return;
    const EmergencyCallState from = m_state;
    m_state = EmergencyCallState::Idle;
    emit stateChanged(m_state, from, QStringLiteral("reset"));
}
