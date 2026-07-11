#include <QTest>

#include "sip/PresenceResubscribePolicy.h"

class TestPresenceResubscribePolicy : public QObject
{
    Q_OBJECT

private slots:
    void noRetryForRejected();
    void noRetryForNoresource();
    void retriesForTimeout();
    void retriesForProbation();
    void retriesForDeactivated();
    void retriesForGiveupAndInvariantAndUnknown();
    void backoffIncreasesWithAttempt();
    void backoffIsCapped();
};

void TestPresenceResubscribePolicy::noRetryForRejected()
{
    QVERIFY(!PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("rejected")));
}

void TestPresenceResubscribePolicy::noRetryForNoresource()
{
    QVERIFY(!PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("noresource")));
}

void TestPresenceResubscribePolicy::retriesForTimeout()
{
    QVERIFY(PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("timeout")));
}

void TestPresenceResubscribePolicy::retriesForProbation()
{
    QVERIFY(PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("probation")));
}

void TestPresenceResubscribePolicy::retriesForDeactivated()
{
    QVERIFY(PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("deactivated")));
}

void TestPresenceResubscribePolicy::retriesForGiveupAndInvariantAndUnknown()
{
    QVERIFY(PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("giveup")));
    QVERIFY(PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("invariant")));
    QVERIFY(PresenceResubscribePolicy::shouldAutoRetry(QStringLiteral("unknown")));
}

void TestPresenceResubscribePolicy::backoffIncreasesWithAttempt()
{
    const int b1 = PresenceResubscribePolicy::backoffMs(1);
    const int b2 = PresenceResubscribePolicy::backoffMs(2);
    const int b3 = PresenceResubscribePolicy::backoffMs(3);
    QCOMPARE(b1, PresenceResubscribePolicy::kBaseBackoffMs);
    QVERIFY(b2 > b1);
    QVERIFY(b3 > b2);
}

void TestPresenceResubscribePolicy::backoffIsCapped()
{
    const int high = PresenceResubscribePolicy::backoffMs(50);
    QCOMPARE(high, PresenceResubscribePolicy::kMaxBackoffMs);
}

QTEST_MAIN(TestPresenceResubscribePolicy)
#include "test_presence_resubscribe_policy.moc"
