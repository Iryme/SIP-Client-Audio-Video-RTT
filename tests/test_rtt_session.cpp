#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>

#include "rtt/RttSession.h"
#include "sip/SipCall.h"
#include "core/Logger.h"

// Unique app name to avoid polluting real QSettings
static const char *k_org  = "IrymeTest_rtt";
static const char *k_app  = "test_rtt_session";

class TestRttSession : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QLatin1String(k_org));
        QCoreApplication::setApplicationName(QLatin1String(k_app));
        // Suppress info/debug noise during tests
        Logger::instance().setLevelEnabled(LogLevel::Info,  false);
        Logger::instance().setLevelEnabled(LogLevel::Debug, false);
    }

    // 1. Initial state is Disabled
    void test_initialStateDisabled()
    {
        RttSession session;
        QCOMPARE(session.state(), RttState::Disabled);
        QCOMPARE(session.isActive(), false);
    }

    // 2. enableForCall transitions to Offered
    void test_enableForCallTransitionsToOffered()
    {
        RttSession session;
        SipCall call;
        QSignalSpy spy(&session, &RttSession::rttStateChanged);

        session.enableForCall(&call);

        QCOMPARE(session.state(), RttState::Offered);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<RttState>(), RttState::Offered);
    }

    // 3. sendText while Disabled drops the text (no crash)
    void test_sendTextWhileDisabledDrops()
    {
        RttSession session;
        QSignalSpy spy(&session, &RttSession::localTextQueued);

        session.sendText(QStringLiteral("Hello"));

        QCOMPARE(spy.count(), 0);  // dropped, not queued
    }

    // 4. sendText while Offered drops the text
    void test_sendTextWhileOfferedDrops()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        QCOMPARE(session.state(), RttState::Offered);

        QSignalSpy spy(&session, &RttSession::localTextQueued);
        session.sendText(QStringLiteral("Hello"));

        QCOMPARE(spy.count(), 0);
    }

    // 5. onCallMediaStateChanged(true) transitions to Active
    void test_mediaActiveTransitionsToActive()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);

        QSignalSpy spy(&session, &RttSession::rttStateChanged);
        session.onCallMediaStateChanged(true);

        QCOMPARE(session.state(), RttState::Active);
        QCOMPARE(session.isActive(), true);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().first().value<RttState>(), RttState::Active);
    }

    // 6. onCallMediaStateChanged(false) from Active goes to Negotiated
    void test_mediaInactiveFromActiveGoesToNegotiated()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        session.onCallMediaStateChanged(true);
        QCOMPARE(session.state(), RttState::Active);

        QSignalSpy spy(&session, &RttSession::rttStateChanged);
        session.onCallMediaStateChanged(false);

        QCOMPARE(session.state(), RttState::Negotiated);
        QCOMPARE(spy.first().first().value<RttState>(), RttState::Negotiated);
    }

    // 7. sendText while Active queues localTextQueued
    void test_sendTextWhileActiveQueues()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        session.onCallMediaStateChanged(true);

        QSignalSpy spy(&session, &RttSession::localTextQueued);
        // sendRttText in stub mode is a no-op but localTextQueued fires first
        session.sendText(QStringLiteral("Hello RTT"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("Hello RTT"));
    }

    // 8. onCallEnded resets to Disabled
    void test_callEndedResetsToDisabled()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        session.onCallMediaStateChanged(true);
        QCOMPARE(session.state(), RttState::Active);

        QSignalSpy spy(&session, &RttSession::rttStateChanged);
        session.onCallEnded();

        QCOMPARE(session.state(), RttState::Disabled);
        QCOMPARE(spy.last().first().value<RttState>(), RttState::Disabled);
    }

    // 9. disable() always resets to Disabled
    void test_disableResetsToDisabled()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        session.onCallMediaStateChanged(true);

        session.disable();

        QCOMPARE(session.state(), RttState::Disabled);
        QCOMPARE(session.isActive(), false);
    }

    // 10. rttStateName returns expected strings
    void test_rttStateName()
    {
        QCOMPARE(rttStateName(RttState::Disabled),   QStringLiteral("Disabled"));
        QCOMPARE(rttStateName(RttState::Offered),    QStringLiteral("Offered"));
        QCOMPARE(rttStateName(RttState::Negotiated), QStringLiteral("Negotiated"));
        QCOMPARE(rttStateName(RttState::Active),     QStringLiteral("Active"));
        QCOMPARE(rttStateName(RttState::Failed),     QStringLiteral("Failed"));
    }

    // 11. Double enableForCall replaces old call cleanly
    void test_doubleEnableReplacesCall()
    {
        RttSession session;
        SipCall call1, call2;

        session.enableForCall(&call1);
        QCOMPARE(session.state(), RttState::Offered);

        session.enableForCall(&call2);
        QCOMPARE(session.state(), RttState::Offered);
    }

    // 12. callDisconnected signal from SipCall triggers onCallEnded
    void test_callDisconnectedSignalCleansUp()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        session.onCallMediaStateChanged(true);
        QCOMPARE(session.state(), RttState::Active);

        // Simulate call disconnected via signal (stub path: reset state machine)
        call.reset(QStringLiteral("test end"));
        // Process queued events from the disconnect signal chain
        QCoreApplication::processEvents();

        // After reset, callDisconnected is emitted internally — session should clear
        QCOMPARE(session.state(), RttState::Disabled);
    }
};

QTEST_GUILESS_MAIN(TestRttSession)
#include "test_rtt_session.moc"
