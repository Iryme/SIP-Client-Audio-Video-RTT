#pragma once

#include <QObject>
#include <QString>

// States of an emergency call lifecycle.
// Transitions are validated by EmergencyCallStateMachine::canTransitionTo().
enum class EmergencyCallState {
    Idle,           // No emergency call in progress
    Preparing,      // Profile validated; controller is setting up resources
    LocationPending,// Waiting for EmergencyLocationProvider to resolve
    ReadyToDial,    // Profile complete; SIP INVITE may be issued (Task 36)
    Dialing,        // INVITE sent; awaiting response (Task 36)
    Active,         // Call connected (Task 36)
    Failed,         // Preparation or call failed
    Ended           // Call ended normally
};

QString emergencyCallStateName(EmergencyCallState state);

// Validates and tracks state transitions for an emergency call.
// Deliberately independent of PJSIP — may be driven by controller or tests.
//
// Valid transition graph:
//   Idle → Preparing
//   Preparing → LocationPending, ReadyToDial, Failed
//   LocationPending → ReadyToDial, Failed
//   ReadyToDial → Dialing, Failed
//   Dialing → Active, Failed
//   Active → Ended, Failed
//   Failed → Idle (reset)
//   Ended → Idle (reset)
class EmergencyCallStateMachine : public QObject
{
    Q_OBJECT
public:
    explicit EmergencyCallStateMachine(QObject *parent = nullptr);

    EmergencyCallState state()     const;
    QString            stateName() const;

    // Returns true if the given target is reachable from the current state.
    bool canTransitionTo(EmergencyCallState to) const;

    // Attempt a transition. Returns true on success, false if rejected.
    bool transition(EmergencyCallState to, const QString &reason = {});

    // Unconditional reset to Idle.
    void reset();

signals:
    void stateChanged(EmergencyCallState newState,
                      EmergencyCallState oldState,
                      const QString      &reason);
    void transitionRejected(EmergencyCallState requested,
                            EmergencyCallState current);

private:
    EmergencyCallState m_state{EmergencyCallState::Idle};
};
