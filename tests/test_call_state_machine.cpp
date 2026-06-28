#include <QtTest/QtTest>

#include "core/Logger.h"
#include "sip/CallStateMachine.h"

// Tests for CallStateMachine in isolation — no SipManager, no PJSIP required.

class TestCallStateMachine : public QObject
{
    Q_OBJECT

private slots:
    void init();

    // State name / display text stability
    void stateNamesAreStable();

    // Outgoing call happy path
    void outgoingCallFlow();

    // Incoming call arrives
    void incomingCallFlow();

    // Answer incoming call: IncomingRinging -> Connecting -> Active
    void answerFlow();

    // Reject incoming call: IncomingRinging -> Idle
    void rejectFlow();

    // Hang up active call: Active -> Disconnecting -> Idle
    void hangupFlow();

    // Hold and resume: Active -> Held -> Active
    void holdResumeFlow();

    // Invalid transitions are rejected with signal and no state change
    void invalidTransitionRejection();

    // reset() returns to Idle from any state
    void disconnectRecovery();

    // After natural Disconnecting -> Idle, a new call can be placed
    void stateResetAfterCallEnd();
};

void TestCallStateMachine::init()
{
    // Logger must be alive; no special setup needed — each test uses its own SM.
}

void TestCallStateMachine::stateNamesAreStable()
{
    QCOMPARE(callStateName(CallState::Idle),            QStringLiteral("Idle"));
    QCOMPARE(callStateName(CallState::OutgoingInit),    QStringLiteral("OutgoingInit"));
    QCOMPARE(callStateName(CallState::Ringing),         QStringLiteral("Ringing"));
    QCOMPARE(callStateName(CallState::IncomingRinging), QStringLiteral("IncomingRinging"));
    QCOMPARE(callStateName(CallState::Connecting),      QStringLiteral("Connecting"));
    QCOMPARE(callStateName(CallState::Active),          QStringLiteral("Active"));
    QCOMPARE(callStateName(CallState::Held),            QStringLiteral("Held"));
    QCOMPARE(callStateName(CallState::Disconnecting),   QStringLiteral("Disconnecting"));
    QCOMPARE(callStateName(CallState::Failed),          QStringLiteral("Failed"));

    QCOMPARE(callStateDisplayText(CallState::Idle),            QStringLiteral("Idle"));
    QCOMPARE(callStateDisplayText(CallState::OutgoingInit),    QStringLiteral("Calling..."));
    QCOMPARE(callStateDisplayText(CallState::Ringing),         QStringLiteral("Ringing..."));
    QCOMPARE(callStateDisplayText(CallState::IncomingRinging), QStringLiteral("Incoming Call"));
    QCOMPARE(callStateDisplayText(CallState::Connecting),      QStringLiteral("Connecting..."));
    QCOMPARE(callStateDisplayText(CallState::Active),          QStringLiteral("Connected"));
    QCOMPARE(callStateDisplayText(CallState::Held),            QStringLiteral("Paused"));
    QCOMPARE(callStateDisplayText(CallState::Disconnecting),   QStringLiteral("Ending..."));
    QCOMPARE(callStateDisplayText(CallState::Failed),          QStringLiteral("Call Failed"));
}

void TestCallStateMachine::outgoingCallFlow()
{
    CallStateMachine sm;
    QSignalSpy stateChangedSpy(&sm, &CallStateMachine::stateChanged);

    // Idle -> OutgoingInit
    QVERIFY(sm.tryTransition(CallState::OutgoingInit, QStringLiteral("Dialing")));
    QCOMPARE(sm.state(), CallState::OutgoingInit);

    // OutgoingInit -> Ringing
    QVERIFY(sm.tryTransition(CallState::Ringing, QStringLiteral("Remote ringing")));
    QCOMPARE(sm.state(), CallState::Ringing);

    // Ringing -> Active (CONFIRMED — some stacks skip Connecting)
    QVERIFY(sm.tryTransition(CallState::Active, QStringLiteral("Call connected"), 200));
    QCOMPARE(sm.state(), CallState::Active);

    // Active -> Disconnecting
    QVERIFY(sm.tryTransition(CallState::Disconnecting, QStringLiteral("User hangup")));
    QCOMPARE(sm.state(), CallState::Disconnecting);

    // Disconnecting -> Idle
    QVERIFY(sm.tryTransition(CallState::Idle, QStringLiteral("Call ended")));
    QCOMPARE(sm.state(), CallState::Idle);

    QCOMPARE(stateChangedSpy.count(), 5);
}

void TestCallStateMachine::incomingCallFlow()
{
    CallStateMachine sm;

    QVERIFY(sm.tryTransition(CallState::IncomingRinging,
                             QStringLiteral("Incoming call from sip:alice@example.com"), 180));
    QCOMPARE(sm.state(), CallState::IncomingRinging);
    QCOMPARE(sm.statusCode(), 180);
}

void TestCallStateMachine::answerFlow()
{
    CallStateMachine sm;

    sm.tryTransition(CallState::IncomingRinging, QStringLiteral("Incoming"));
    QCOMPARE(sm.state(), CallState::IncomingRinging);

    // IncomingRinging -> Connecting (answer() starts media negotiation)
    QVERIFY(sm.tryTransition(CallState::Connecting, QStringLiteral("Answering"), 200));
    QCOMPARE(sm.state(), CallState::Connecting);

    // Connecting -> Active (media up / CONFIRMED)
    QVERIFY(sm.tryTransition(CallState::Active, QStringLiteral("Call connected"), 200));
    QCOMPARE(sm.state(), CallState::Active);
}

void TestCallStateMachine::rejectFlow()
{
    CallStateMachine sm;

    sm.tryTransition(CallState::IncomingRinging, QStringLiteral("Incoming"));
    QCOMPARE(sm.state(), CallState::IncomingRinging);

    // IncomingRinging -> Idle (local reject, sends 486)
    QVERIFY(sm.tryTransition(CallState::Idle, QStringLiteral("Rejected by local user"), 486));
    QCOMPARE(sm.state(), CallState::Idle);
    QCOMPARE(sm.statusCode(), 486);
}

void TestCallStateMachine::hangupFlow()
{
    CallStateMachine sm;

    sm.tryTransition(CallState::OutgoingInit, QStringLiteral("Dialing"));
    sm.tryTransition(CallState::Ringing,      QStringLiteral("Remote ringing"));
    sm.tryTransition(CallState::Active,       QStringLiteral("Connected"), 200);
    QCOMPARE(sm.state(), CallState::Active);

    // Active -> Disconnecting
    QVERIFY(sm.tryTransition(CallState::Disconnecting, QStringLiteral("User hangup")));
    QCOMPARE(sm.state(), CallState::Disconnecting);

    // Disconnecting -> Idle
    QVERIFY(sm.tryTransition(CallState::Idle, QStringLiteral("Call ended"), 0));
    QCOMPARE(sm.state(), CallState::Idle);
}

void TestCallStateMachine::holdResumeFlow()
{
    CallStateMachine sm;

    sm.tryTransition(CallState::OutgoingInit, QStringLiteral("Dialing"));
    sm.tryTransition(CallState::Ringing,      QStringLiteral("Remote ringing"));
    sm.tryTransition(CallState::Active,       QStringLiteral("Connected"), 200);
    QCOMPARE(sm.state(), CallState::Active);

    // Active -> Held
    QVERIFY(sm.tryTransition(CallState::Held, QStringLiteral("On hold")));
    QCOMPARE(sm.state(), CallState::Held);

    // Held -> Active
    QVERIFY(sm.tryTransition(CallState::Active, QStringLiteral("Resumed")));
    QCOMPARE(sm.state(), CallState::Active);
}

void TestCallStateMachine::invalidTransitionRejection()
{
    CallStateMachine sm;
    QSignalSpy rejectedSpy(&sm, &CallStateMachine::transitionRejected);
    QSignalSpy changedSpy( &sm, &CallStateMachine::stateChanged);

    // Idle -> Active (must go through OutgoingInit or IncomingRinging first)
    QVERIFY(!sm.tryTransition(CallState::Active, QStringLiteral("bad")));
    QCOMPARE(sm.state(), CallState::Idle);

    // Idle -> Held (nonsensical)
    QVERIFY(!sm.tryTransition(CallState::Held, QStringLiteral("bad")));
    QCOMPARE(sm.state(), CallState::Idle);

    sm.tryTransition(CallState::OutgoingInit, QStringLiteral("Dialing"));
    sm.tryTransition(CallState::Ringing,      QStringLiteral("Remote ringing"));
    sm.tryTransition(CallState::Active,       QStringLiteral("Connected"), 200);

    // Active -> OutgoingInit (can't start a new call while one is active)
    QVERIFY(!sm.tryTransition(CallState::OutgoingInit, QStringLiteral("bad")));
    QCOMPARE(sm.state(), CallState::Active);

    // Active -> IncomingRinging (nonsensical)
    QVERIFY(!sm.tryTransition(CallState::IncomingRinging, QStringLiteral("bad")));
    QCOMPARE(sm.state(), CallState::Active);

    // Active -> Ringing (cannot go back to ringing)
    QVERIFY(!sm.tryTransition(CallState::Ringing, QStringLiteral("bad")));
    QCOMPARE(sm.state(), CallState::Active);

    QCOMPARE(rejectedSpy.count(), 5);
    QCOMPARE(changedSpy.count(), 3); // OutgoingInit + Ringing + Active
}

void TestCallStateMachine::disconnectRecovery()
{
    CallStateMachine sm;

    sm.tryTransition(CallState::OutgoingInit, QStringLiteral("Dialing"));
    sm.tryTransition(CallState::Ringing,      QStringLiteral("Remote ringing"));
    sm.tryTransition(CallState::Active,       QStringLiteral("Connected"), 200);
    QCOMPARE(sm.state(), CallState::Active);

    QSignalSpy changedSpy(&sm, &CallStateMachine::stateChanged);

    // reset() unconditionally returns to Idle from any state
    sm.reset(QStringLiteral("Emergency shutdown"));
    QCOMPARE(sm.state(), CallState::Idle);
    QCOMPARE(changedSpy.count(), 1);

    const auto &args = changedSpy.at(0);
    QCOMPARE(args.at(0).value<CallState>(), CallState::Idle);
}

void TestCallStateMachine::stateResetAfterCallEnd()
{
    CallStateMachine sm;

    // Complete a full call lifecycle
    sm.tryTransition(CallState::OutgoingInit,  QStringLiteral("Dialing"));
    sm.tryTransition(CallState::Ringing,       QStringLiteral("Remote ringing"));
    sm.tryTransition(CallState::Active,        QStringLiteral("Connected"), 200);
    sm.tryTransition(CallState::Disconnecting, QStringLiteral("Hangup"));
    sm.tryTransition(CallState::Idle,          QStringLiteral("Call ended"));

    QCOMPARE(sm.state(), CallState::Idle);

    // State machine is back in Idle — a new outgoing call must be possible
    QVERIFY(sm.tryTransition(CallState::OutgoingInit, QStringLiteral("New call")));
    QCOMPARE(sm.state(), CallState::OutgoingInit);
}

QTEST_GUILESS_MAIN(TestCallStateMachine)
#include "test_call_state_machine.moc"
