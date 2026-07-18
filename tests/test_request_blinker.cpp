#include <QtTest/QtTest>
#include <QSignalSpy>

#include "gui/widgets/RequestBlinker.h"

// Tests for RequestBlinker -- the shared flashing/pulsing indicator behind
// CallWorkspacePanel's Request Video and Request RTT alert parity (Task
// W113E). No widget/style/SIP knowledge here, just the timer+bool state
// machine itself.

class TestRequestBlinker : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToOff();
    void startTurnsOnImmediately();
    void startEmitsToggledImmediately();
    void startIsIdempotent();
    void stopTurnsOff();
    void stopEmitsToggledWhenItWasOn();
    void stopDoesNotEmitWhenAlreadyOff();
    void stopIsIdempotent();
    void timerTogglesOnEachTick();
    void restartAfterStopStartsFromOn();
};

void TestRequestBlinker::defaultsToOff()
{
    RequestBlinker blinker;
    QVERIFY(!blinker.isOn());
    QVERIFY(!blinker.isActive());
}

void TestRequestBlinker::startTurnsOnImmediately()
{
    RequestBlinker blinker;
    blinker.start();
    QVERIFY(blinker.isOn());
    QVERIFY(blinker.isActive());
}

void TestRequestBlinker::startEmitsToggledImmediately()
{
    RequestBlinker blinker;
    QSignalSpy spy(&blinker, &RequestBlinker::toggled);
    blinker.start();
    // Fires once synchronously so the caller's UI doesn't have to wait for
    // the first 500ms tick before showing the alert.
    QCOMPARE(spy.count(), 1);
}

void TestRequestBlinker::startIsIdempotent()
{
    RequestBlinker blinker;
    blinker.start();
    blinker.start();
    blinker.start();
    // No duplicate timers -- still just one QTimer under the hood, still on.
    QVERIFY(blinker.isOn());
    QVERIFY(blinker.isActive());
}

void TestRequestBlinker::stopTurnsOff()
{
    RequestBlinker blinker;
    blinker.start();
    blinker.stop();
    QVERIFY(!blinker.isOn());
    QVERIFY(!blinker.isActive());
}

void TestRequestBlinker::stopEmitsToggledWhenItWasOn()
{
    RequestBlinker blinker;
    blinker.start();
    QSignalSpy spy(&blinker, &RequestBlinker::toggled);
    blinker.stop();
    QCOMPARE(spy.count(), 1);
    QVERIFY(!blinker.isOn());
}

void TestRequestBlinker::stopDoesNotEmitWhenAlreadyOff()
{
    RequestBlinker blinker;
    QSignalSpy spy(&blinker, &RequestBlinker::toggled);
    blinker.stop();
    QCOMPARE(spy.count(), 0);
}

void TestRequestBlinker::stopIsIdempotent()
{
    RequestBlinker blinker;
    blinker.stop();
    blinker.stop();
    QVERIFY(!blinker.isOn());
    QVERIFY(!blinker.isActive());
}

void TestRequestBlinker::timerTogglesOnEachTick()
{
    RequestBlinker blinker;
    blinker.start();
    QVERIFY(blinker.isOn());

    QSignalSpy spy(&blinker, &RequestBlinker::toggled);
    // Interval is 500ms; wait long enough for at least two ticks.
    QVERIFY(spy.wait(1200));
    QVERIFY(!blinker.isOn());
    QVERIFY(spy.wait(1200));
    QVERIFY(blinker.isOn());
}

void TestRequestBlinker::restartAfterStopStartsFromOn()
{
    RequestBlinker blinker;
    blinker.start();
    blinker.stop();
    QVERIFY(!blinker.isOn());
    blinker.start();
    QVERIFY(blinker.isOn());
}

QTEST_GUILESS_MAIN(TestRequestBlinker)
#include "test_request_blinker.moc"
