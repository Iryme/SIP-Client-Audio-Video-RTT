#include <QtTest/QtTest>

#include "sip/TypingIndicatorController.h"

// Tests for TypingIndicatorController (Task W097) — the local outbound
// RFC 3994 typing-state machine. Pure Qt/QTimer, no PJSIP/network
// dependency. Timer-driven transitions are exercised via the test-only
// trigger*Timeout() hooks (same pattern as SipManager::scheduleRefresh)
// instead of waiting real wall-clock seconds, keeping the suite fast and
// deterministic.

class TestTypingIndicatorController : public QObject
{
    Q_OBJECT

private slots:
    void firstTypingSendsActive();
    void repeatedTypingDoesNotResendActive();
    void refreshTimeoutResendsActiveWhileStillActive();
    void refreshTimeoutNoOpWhenNotActive();
    void idleTimeoutTransitionsToIdle();
    void idleTimeoutNoOpWhenNotActive();
    void goneTimeoutAfterIdleSendsGone();
    void goneTimeoutNoOpWhenNotIdle();
    void resumeTypingAfterIdleSendsActiveAgain();
    void stopSendsGoneOnce();
    void stopWhenNeverStartedIsNoOp();
    void configRefreshSecondsIsReflectedInActivePayload();
    void emptyTextChangeIsNoOp();
};

namespace {
TypingIndicatorController::Config testConfig()
{
    TypingIndicatorController::Config cfg;
    cfg.refreshSeconds = 60;
    cfg.idleSeconds = 15;
    cfg.goneDelaySeconds = 30;
    return cfg;
}
} // namespace

void TestTypingIndicatorController::firstTypingSendsActive()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<IsComposingInfo::State>(), IsComposingInfo::State::Active);
    QCOMPARE(ctrl.currentState(), IsComposingInfo::State::Active);
}

void TestTypingIndicatorController::repeatedTypingDoesNotResendActive()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);
    ctrl.onTextChanged(true);
    ctrl.onTextChanged(true);
    ctrl.onTextChanged(true);

    // Debounce: still Active the whole time — only the first keystroke
    // sent a notification.
    QCOMPARE(spy.count(), 1);
}

void TestTypingIndicatorController::refreshTimeoutResendsActiveWhileStillActive()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);
    ctrl.triggerRefreshTimeout();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).value<IsComposingInfo::State>(), IsComposingInfo::State::Active);
    QCOMPARE(ctrl.currentState(), IsComposingInfo::State::Active);
}

void TestTypingIndicatorController::refreshTimeoutNoOpWhenNotActive()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.triggerRefreshTimeout();

    QCOMPARE(spy.count(), 0);
}

void TestTypingIndicatorController::idleTimeoutTransitionsToIdle()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);
    ctrl.triggerIdleTimeout();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).value<IsComposingInfo::State>(), IsComposingInfo::State::Idle);
    QCOMPARE(ctrl.currentState(), IsComposingInfo::State::Idle);
}

void TestTypingIndicatorController::idleTimeoutNoOpWhenNotActive()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.triggerIdleTimeout();

    QCOMPARE(spy.count(), 0);
}

void TestTypingIndicatorController::goneTimeoutAfterIdleSendsGone()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);
    ctrl.triggerIdleTimeout();
    ctrl.triggerGoneTimeout();

    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).value<IsComposingInfo::State>(), IsComposingInfo::State::Gone);
    QCOMPARE(ctrl.currentState(), IsComposingInfo::State::Unknown);
}

void TestTypingIndicatorController::goneTimeoutNoOpWhenNotIdle()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true); // Active, not Idle
    ctrl.triggerGoneTimeout();

    QCOMPARE(spy.count(), 1); // only the initial Active — goneTimeout was a no-op
}

void TestTypingIndicatorController::resumeTypingAfterIdleSendsActiveAgain()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);
    ctrl.triggerIdleTimeout();
    ctrl.onTextChanged(true);

    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).value<IsComposingInfo::State>(), IsComposingInfo::State::Active);
    QCOMPARE(ctrl.currentState(), IsComposingInfo::State::Active);
}

void TestTypingIndicatorController::stopSendsGoneOnce()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);
    ctrl.stop();
    ctrl.stop(); // duplicate suppression — must not send a second "gone"

    QCOMPARE(spy.count(), 2); // Active + Gone
    QCOMPARE(spy.at(1).at(0).value<IsComposingInfo::State>(), IsComposingInfo::State::Gone);
}

void TestTypingIndicatorController::stopWhenNeverStartedIsNoOp()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.stop();

    QCOMPARE(spy.count(), 0);
}

void TestTypingIndicatorController::configRefreshSecondsIsReflectedInActivePayload()
{
    TypingIndicatorController ctrl;
    TypingIndicatorController::Config cfg = testConfig();
    cfg.refreshSeconds = 42;
    ctrl.setConfig(cfg);
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(true);

    QCOMPARE(spy.at(0).at(1).toInt(), 42);
}

void TestTypingIndicatorController::emptyTextChangeIsNoOp()
{
    TypingIndicatorController ctrl;
    ctrl.setConfig(testConfig());
    QSignalSpy spy(&ctrl, &TypingIndicatorController::sendIsComposingRequested);

    ctrl.onTextChanged(false);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(ctrl.currentState(), IsComposingInfo::State::Unknown);
}

QTEST_GUILESS_MAIN(TestTypingIndicatorController)
#include "test_typing_indicator_controller.moc"
