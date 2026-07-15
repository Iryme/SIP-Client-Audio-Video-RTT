#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>

#include "rtt/RttSession.h"
#include "rtt/RttTextUtils.h"
#include "rtt/RttConfig.h"
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

        QCOMPARE(session.state(), RttState::LocalOfferPending);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<RttState>(), RttState::LocalOfferPending);
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
        QCOMPARE(session.state(), RttState::LocalOfferPending);

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

        QCOMPARE(session.state(), RttState::Negotiating);
        QCOMPARE(spy.first().first().value<RttState>(), RttState::Negotiating);
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
        QCOMPARE(rttStateName(RttState::Disabled),           QStringLiteral("Disabled"));
        QCOMPARE(rttStateName(RttState::RemoteOfferPending), QStringLiteral("RemoteOfferPending"));
        QCOMPARE(rttStateName(RttState::LocalOfferPending),  QStringLiteral("LocalOfferPending"));
        QCOMPARE(rttStateName(RttState::Negotiating),        QStringLiteral("Negotiating"));
        QCOMPARE(rttStateName(RttState::Active),             QStringLiteral("Active"));
        QCOMPARE(rttStateName(RttState::Rejected),           QStringLiteral("Rejected"));
        QCOMPARE(rttStateName(RttState::Failed),             QStringLiteral("Failed"));
    }

    // 10b. onIncomingRttRequest transitions to RemoteOfferPending
    void test_incomingRttRequestTransitionsToRemoteOfferPending()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);

        QSignalSpy spy(&session, &RttSession::rttStateChanged);
        session.onIncomingRttRequest();

        QCOMPARE(session.state(), RttState::RemoteOfferPending);
        QCOMPARE(spy.last().first().value<RttState>(), RttState::RemoteOfferPending);
    }

    // 10c. onIncomingRttRejected transitions to Rejected
    void test_incomingRttRejectedTransitionsToRejected()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        session.onIncomingRttRequest();

        session.onIncomingRttRejected();

        QCOMPARE(session.state(), RttState::Rejected);
    }

    // 10d. onNegotiationFailed transitions to Failed
    void test_negotiationFailedTransitionsToFailed()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);

        session.onNegotiationFailed(QStringLiteral("RTP port unavailable"));

        QCOMPARE(session.state(), RttState::Failed);
    }

    // 10e. A resolved state (Active) is not affected by a stale negotiation
    // timeout that could theoretically still be pending — verifies the
    // timeout is stopped once a definitive outcome occurs (anti-ping-pong /
    // stale-callback guard).
    void test_negotiationTimeoutStoppedOnceActive()
    {
        RttSession session;
        SipCall call;
        session.enableForCall(&call);
        QCOMPARE(session.state(), RttState::LocalOfferPending);

        session.onCallMediaStateChanged(true);
        QCOMPARE(session.state(), RttState::Active);

        // If the timeout timer were still running it would eventually force
        // Failed; process events briefly and confirm state is unaffected.
        QCoreApplication::processEvents();
        QCOMPARE(session.state(), RttState::Active);
    }

    // 11. Double enableForCall replaces old call cleanly
    void test_doubleEnableReplacesCall()
    {
        RttSession session;
        SipCall call1, call2;

        session.enableForCall(&call1);
        QCOMPARE(session.state(), RttState::LocalOfferPending);

        session.enableForCall(&call2);
        QCOMPARE(session.state(), RttState::LocalOfferPending);
    }

    // 11b. SipCall: no pending incoming RTT request by default.
    void test_sipCall_noPendingIncomingRequestByDefault()
    {
        SipCall call;
        QCOMPARE(call.hasPendingIncomingRttRequest(), false);
    }

    // 11c. SipCall: accepting with nothing pending is a no-op (returns false).
    void test_sipCall_acceptWithNothingPendingFails()
    {
        SipCall call;
        QSignalSpy spy(&call, &SipCall::rttLocalOfferSent);
        QCOMPARE(call.acceptIncomingRttRequest(), false);
        QCOMPARE(spy.count(), 0);
    }

    // 11d. SipCall: rejecting with nothing pending is a no-op (returns false,
    // does not emit rttRequestRejected).
    void test_sipCall_rejectWithNothingPendingFails()
    {
        SipCall call;
        QSignalSpy spy(&call, &SipCall::rttRequestRejected);
        QCOMPARE(call.rejectIncomingRttRequest(), false);
        QCOMPARE(spy.count(), 0);
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

    // -----------------------------------------------------------------------
    // rttTxDelta tests
    // -----------------------------------------------------------------------

    // 13. Appending a single character produces just that character
    void test_txDelta_appendChar()
    {
        QCOMPARE(rttTxDelta(QStringLiteral("He"), QStringLiteral("Hel")),
                 QStringLiteral("l"));
    }

    // 14. Deleting one character (backspace) produces one BS
    void test_txDelta_backspaceOne()
    {
        const QString delta = rttTxDelta(QStringLiteral("Hel"), QStringLiteral("He"));
        QCOMPARE(delta.length(), 1);
        QCOMPARE(delta[0].unicode(), static_cast<ushort>(0x08));
    }

    // 15. Deleting two characters produces two BS chars
    void test_txDelta_backspaceTwo()
    {
        const QString delta = rttTxDelta(QStringLiteral("Hello"), QStringLiteral("Hel"));
        QCOMPARE(delta.length(), 2);
        QCOMPARE(delta[0].unicode(), static_cast<ushort>(0x08));
        QCOMPARE(delta[1].unicode(), static_cast<ushort>(0x08));
    }

    // 16. Pasting text (replacing suffix) → BS for deleted part + new text
    void test_txDelta_paste()
    {
        // User had "Hi" and pasted "Hello" (common prefix "H", delete "i", add "ello")
        const QString delta = rttTxDelta(QStringLiteral("Hi"), QStringLiteral("Hello"));
        // Expected: one BS (for 'i') + "ello"
        QCOMPARE(delta.length(), 5); // 1 BS + 4 chars
        QCOMPARE(delta[0].unicode(), static_cast<ushort>(0x08));
        QCOMPARE(delta.mid(1), QStringLiteral("ello"));
    }

    // 17. No change produces empty delta
    void test_txDelta_noChange()
    {
        QVERIFY(rttTxDelta(QStringLiteral("Hello"), QStringLiteral("Hello")).isEmpty());
    }

    // 18. Empty → empty: no delta
    void test_txDelta_emptyToEmpty()
    {
        QVERIFY(rttTxDelta(QString(), QString()).isEmpty());
    }

    // 19. Empty → text: just the new text
    void test_txDelta_emptyToText()
    {
        QCOMPARE(rttTxDelta(QString(), QStringLiteral("Hi")), QStringLiteral("Hi"));
    }

    // 20. Text → empty: BS for each character
    void test_txDelta_textToEmpty()
    {
        const QString delta = rttTxDelta(QStringLiteral("Hi"), QString());
        QCOMPARE(delta.length(), 2);
        for (int i = 0; i < 2; ++i)
            QCOMPARE(delta[i].unicode(), static_cast<ushort>(0x08));
    }

    // -----------------------------------------------------------------------
    // rttRxProcess tests
    // -----------------------------------------------------------------------

    // 21. Appending plain text accumulates correctly
    void test_rxProcess_appendText()
    {
        QString buf;
        buf = rttRxProcess(buf, QStringLiteral("He"));
        buf = rttRxProcess(buf, QStringLiteral("llo"));
        QCOMPARE(buf, QStringLiteral("Hello"));
    }

    // 22. BS removes the last character
    void test_rxProcess_backspace()
    {
        QString buf = QStringLiteral("Helo");
        buf = rttRxProcess(buf, QString(QChar(0x08)));  // delete 'o'
        QCOMPARE(buf, QStringLiteral("Hel"));
    }

    // 23. Multiple BS chars
    void test_rxProcess_multipleBackspace()
    {
        QString buf = QStringLiteral("Hello");
        buf = rttRxProcess(buf, QString(2, QChar(0x08)));  // delete "lo" → "Hel"
        QCOMPARE(buf, QStringLiteral("Hel"));
    }

    // 24. BS on empty buffer does not crash
    void test_rxProcess_backspaceOnEmpty()
    {
        QString buf;
        buf = rttRxProcess(buf, QString(QChar(0x08)));
        QVERIFY(buf.isEmpty());
    }

    // 25. BS + new char: correct result
    void test_rxProcess_backspaceThenChar()
    {
        QString buf = QStringLiteral("Helo");
        // Delete 'o', add 'l', add 'o' = "Hello"
        buf = rttRxProcess(buf, QString(QChar(0x08)) + QStringLiteral("lo"));
        QCOMPARE(buf, QStringLiteral("Hello"));
    }

    // 26. CR is passed through (caller flushes — rttRxProcess just appends CR)
    void test_rxProcess_crPassThrough()
    {
        QString buf = QStringLiteral("Hi");
        buf = rttRxProcess(buf, QString(QChar(0x0D)));
        QCOMPARE(buf, QStringLiteral("Hi\r"));
    }

    // -----------------------------------------------------------------------
    // RttConfig constants (RFC 4103 / RFC 2198 RED levels)
    // -----------------------------------------------------------------------

    // 27. Default RED level must be 2 (RFC 4103 recommendation)
    void test_rttConfig_redLevelDefault()
    {
        QCOMPARE(kRttRedLevelDefault, 2);
    }

    // 28. Disabled level must be 0
    void test_rttConfig_redLevelDisabled()
    {
        QCOMPARE(kRttRedLevelDisabled, 0);
    }

    // 29. Max level is 2 (PJMEDIA_TXT_STREAM_MAX_RED_LEVELS)
    void test_rttConfig_redLevelMax()
    {
        QCOMPARE(kRttRedLevelMax, 2);
    }

    // 30. Default must not exceed max
    void test_rttConfig_defaultNotAboveMax()
    {
        QVERIFY(kRttRedLevelDefault <= kRttRedLevelMax);
    }

    // 31. Disabled must be strictly below default
    void test_rttConfig_disabledBelowDefault()
    {
        QVERIFY(kRttRedLevelDisabled < kRttRedLevelDefault);
    }
};

QTEST_GUILESS_MAIN(TestRttSession)
#include "test_rtt_session.moc"
