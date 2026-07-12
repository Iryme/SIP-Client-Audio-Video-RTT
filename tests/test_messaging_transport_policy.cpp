#include <QtTest/QtTest>

#include "msrp/MessagingTransportPolicy.h"

class TestMessagingTransportPolicy : public QObject
{
    Q_OBJECT

private slots:
    void automaticUsesMsrpWhenEstablished();
    void automaticFallsBackWhenUnavailable();
    void automaticNotAllowedWhenFallbackDisabled();
    void msrpRequiredUnavailableIsError();
    void msrpRequiredUsesMsrpWhenEstablished();
    void sipMessageOnlyNeverUsesMsrp();
    void msrpPreferredBehavesLikeAutomatic();
    void fallbackAfterFailurePermittedForAutomatic();
    void fallbackAfterFailureDeniedForRequired();
    void fallbackAfterFailureDeniedWhenDisabled();
};

void TestMessagingTransportPolicy::automaticUsesMsrpWhenEstablished()
{
    const auto d = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::Automatic, true, true);
    QVERIFY(d.allowed);
    QCOMPARE(d.transport, MessagingActualTransport::Msrp);
}

void TestMessagingTransportPolicy::automaticFallsBackWhenUnavailable()
{
    const auto d = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::Automatic, false, true);
    QVERIFY(d.allowed);
    QCOMPARE(d.transport, MessagingActualTransport::SipMessageFallback);
}

void TestMessagingTransportPolicy::automaticNotAllowedWhenFallbackDisabled()
{
    const auto d = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::Automatic, false, false);
    QVERIFY(!d.allowed);
}

void TestMessagingTransportPolicy::msrpRequiredUnavailableIsError()
{
    const auto d = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::MsrpRequired, false, true);
    QVERIFY(!d.allowed);
    QVERIFY(!d.reason.isEmpty());
}

void TestMessagingTransportPolicy::msrpRequiredUsesMsrpWhenEstablished()
{
    const auto d = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::MsrpRequired, true, true);
    QVERIFY(d.allowed);
    QCOMPARE(d.transport, MessagingActualTransport::Msrp);
}

void TestMessagingTransportPolicy::sipMessageOnlyNeverUsesMsrp()
{
    const auto d = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::SipMessageOnly, true, true);
    QVERIFY(d.allowed);
    QCOMPARE(d.transport, MessagingActualTransport::SipMessage);
}

void TestMessagingTransportPolicy::msrpPreferredBehavesLikeAutomatic()
{
    const auto d1 = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::MsrpPreferred, true, true);
    QCOMPARE(d1.transport, MessagingActualTransport::Msrp);
    const auto d2 = MessagingTransportPolicy::decideInitialTransport(MessagingTransportMode::MsrpPreferred, false, true);
    QCOMPARE(d2.transport, MessagingActualTransport::SipMessageFallback);
}

void TestMessagingTransportPolicy::fallbackAfterFailurePermittedForAutomatic()
{
    const auto d = MessagingTransportPolicy::decideFallbackAfterMsrpFailure(MessagingTransportMode::Automatic, true);
    QVERIFY(d.allowed);
    QCOMPARE(d.transport, MessagingActualTransport::SipMessageFallback);
}

void TestMessagingTransportPolicy::fallbackAfterFailureDeniedForRequired()
{
    const auto d = MessagingTransportPolicy::decideFallbackAfterMsrpFailure(MessagingTransportMode::MsrpRequired, true);
    QVERIFY(!d.allowed);
}

void TestMessagingTransportPolicy::fallbackAfterFailureDeniedWhenDisabled()
{
    const auto d = MessagingTransportPolicy::decideFallbackAfterMsrpFailure(MessagingTransportMode::Automatic, false);
    QVERIFY(!d.allowed);
}

QTEST_GUILESS_MAIN(TestMessagingTransportPolicy)
#include "test_messaging_transport_policy.moc"
