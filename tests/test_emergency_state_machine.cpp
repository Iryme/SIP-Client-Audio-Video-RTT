#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>

#include "emergency/EmergencyCallStateMachine.h"

class TestEmergencyCallStateMachine : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("IrymeTest_emergency"));
        QCoreApplication::setApplicationName(QStringLiteral("test_emergency_state_machine"));
    }

    // 1. Initial state is Idle
    void test_initialState()
    {
        EmergencyCallStateMachine sm;
        QCOMPARE(sm.state(), EmergencyCallState::Idle);
        QCOMPARE(sm.stateName(), QStringLiteral("Idle"));
    }

    // 2. Idle → Preparing is valid
    void test_idleToPreparing()
    {
        EmergencyCallStateMachine sm;
        QSignalSpy spy(&sm, &EmergencyCallStateMachine::stateChanged);

        const bool ok = sm.transition(EmergencyCallState::Preparing, QStringLiteral("test"));
        QVERIFY(ok);
        QCOMPARE(sm.state(), EmergencyCallState::Preparing);
        QCOMPARE(spy.count(), 1);
    }

    // 3. Idle → ReadyToDial is invalid
    void test_idleToReadyToDialRejected()
    {
        EmergencyCallStateMachine sm;
        QSignalSpy spy(&sm, &EmergencyCallStateMachine::transitionRejected);

        const bool ok = sm.transition(EmergencyCallState::ReadyToDial);
        QVERIFY(!ok);
        QCOMPARE(sm.state(), EmergencyCallState::Idle);
        QCOMPARE(spy.count(), 1);
    }

    // 4. Full happy path: Idle→Preparing→ReadyToDial→Dialing→Active→Ended→Idle
    void test_fullHappyPath()
    {
        EmergencyCallStateMachine sm;
        using S = EmergencyCallState;

        QVERIFY(sm.transition(S::Preparing));
        QVERIFY(sm.transition(S::ReadyToDial));
        QVERIFY(sm.transition(S::Dialing));
        QVERIFY(sm.transition(S::Active));
        QVERIFY(sm.transition(S::Ended));
        QVERIFY(sm.transition(S::Idle));

        QCOMPARE(sm.state(), S::Idle);
    }

    // 5. LocationPending path: Idle→Preparing→LocationPending→ReadyToDial
    void test_locationPendingPath()
    {
        EmergencyCallStateMachine sm;
        using S = EmergencyCallState;

        QVERIFY(sm.transition(S::Preparing));
        QVERIFY(sm.transition(S::LocationPending));
        QCOMPARE(sm.state(), S::LocationPending);
        QVERIFY(sm.transition(S::ReadyToDial));
        QCOMPARE(sm.state(), S::ReadyToDial);
    }

    // 6. Failed path from any active state → Idle
    void test_failedPathFromPreparing()
    {
        EmergencyCallStateMachine sm;
        using S = EmergencyCallState;

        QVERIFY(sm.transition(S::Preparing));
        QVERIFY(sm.transition(S::Failed));
        QCOMPARE(sm.state(), S::Failed);
        QVERIFY(sm.transition(S::Idle));
        QCOMPARE(sm.state(), S::Idle);
    }

    // 7. reset() unconditionally returns to Idle
    void test_resetFromActive()
    {
        EmergencyCallStateMachine sm;
        using S = EmergencyCallState;

        QVERIFY(sm.transition(S::Preparing));
        QVERIFY(sm.transition(S::ReadyToDial));
        QVERIFY(sm.transition(S::Dialing));
        QVERIFY(sm.transition(S::Active));

        sm.reset();
        QCOMPARE(sm.state(), S::Idle);
    }

    // 8. reset() from Idle is a no-op (no signal)
    void test_resetFromIdleNoSignal()
    {
        EmergencyCallStateMachine sm;
        QSignalSpy spy(&sm, &EmergencyCallStateMachine::stateChanged);

        sm.reset();
        QCOMPARE(spy.count(), 0);
        QCOMPARE(sm.state(), EmergencyCallState::Idle);
    }

    // 9. canTransitionTo() reflects valid edges without side effects
    void test_canTransitionTo()
    {
        EmergencyCallStateMachine sm;
        using S = EmergencyCallState;

        QVERIFY(sm.canTransitionTo(S::Preparing));
        QVERIFY(!sm.canTransitionTo(S::Active));
        QVERIFY(!sm.canTransitionTo(S::Dialing));

        sm.transition(S::Preparing);
        QVERIFY(sm.canTransitionTo(S::LocationPending));
        QVERIFY(sm.canTransitionTo(S::ReadyToDial));
        QVERIFY(sm.canTransitionTo(S::Failed));
        QVERIFY(!sm.canTransitionTo(S::Idle));
    }

    // 10. stateChanged signal carries correct old/new states
    void test_stateChangedSignalPayload()
    {
        EmergencyCallStateMachine sm;
        QSignalSpy spy(&sm, &EmergencyCallStateMachine::stateChanged);

        sm.transition(EmergencyCallState::Preparing, QStringLiteral("reason_a"));
        QCOMPARE(spy.count(), 1);

        const QList<QVariant> args = spy.first();
        QCOMPARE(args[0].value<EmergencyCallState>(), EmergencyCallState::Preparing);
        QCOMPARE(args[1].value<EmergencyCallState>(), EmergencyCallState::Idle);
        QCOMPARE(args[2].toString(), QStringLiteral("reason_a"));
    }

    // 11. stateName() returns correct strings for all states
    void test_stateNames()
    {
        QCOMPARE(emergencyCallStateName(EmergencyCallState::Idle),            QStringLiteral("Idle"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::Preparing),       QStringLiteral("Preparing"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::LocationPending),  QStringLiteral("LocationPending"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::ReadyToDial),     QStringLiteral("ReadyToDial"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::Dialing),         QStringLiteral("Dialing"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::Active),          QStringLiteral("Active"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::Failed),          QStringLiteral("Failed"));
        QCOMPARE(emergencyCallStateName(EmergencyCallState::Ended),           QStringLiteral("Ended"));
    }
};

QTEST_GUILESS_MAIN(TestEmergencyCallStateMachine)
#include "test_emergency_state_machine.moc"
