#include <QtTest/QtTest>

#include "msrp/MsrpTransactionStore.h"

class TestMsrpTransactionStore : public QObject
{
    Q_OBJECT

private slots:
    void beginCreatesQueuedTransaction();
    void updateStatusToAccepted();
    void applyReportSuccess();
    void applyReportFailure();
    void unknownTransactionIdUpdateReturnsFalse();
    void pendingCountExcludesTerminal();
    void clearRemovesAll();
};

void TestMsrpTransactionStore::beginCreatesQueuedTransaction()
{
    MsrpTransactionStore store;
    const auto t = store.begin(QStringLiteral("tid1"), QStringLiteral("sess1"), MsrpMethod::Send, QStringLiteral("m1"), true);
    QCOMPARE(t.status, MsrpTransactionStatus::Queued);
    QCOMPARE(store.all().size(), 1);
}

void TestMsrpTransactionStore::updateStatusToAccepted()
{
    MsrpTransactionStore store;
    store.begin(QStringLiteral("tid1"), QStringLiteral("sess1"), MsrpMethod::Send, QStringLiteral("m1"), true);
    QVERIFY(store.updateStatus(QStringLiteral("tid1"), MsrpTransactionStatus::Accepted, 200, QStringLiteral("OK")));
    QCOMPARE(store.all().first().status, MsrpTransactionStatus::Accepted);
    QCOMPARE(store.all().first().responseCode, 200);
}

void TestMsrpTransactionStore::applyReportSuccess()
{
    MsrpTransactionStore store;
    store.begin(QStringLiteral("tid1"), QStringLiteral("sess1"), MsrpMethod::Send, QStringLiteral("m1"), true);
    QVERIFY(store.applyReport(QStringLiteral("tid1"), QStringLiteral("000 200 OK"), true));
    QCOMPARE(store.all().first().status, MsrpTransactionStatus::ReportedSuccess);
}

void TestMsrpTransactionStore::applyReportFailure()
{
    MsrpTransactionStore store;
    store.begin(QStringLiteral("tid1"), QStringLiteral("sess1"), MsrpMethod::Send, QStringLiteral("m1"), true);
    QVERIFY(store.applyReport(QStringLiteral("tid1"), QStringLiteral("000 481 Unknown"), false));
    QCOMPARE(store.all().first().status, MsrpTransactionStatus::ReportedFailure);
}

void TestMsrpTransactionStore::unknownTransactionIdUpdateReturnsFalse()
{
    MsrpTransactionStore store;
    QVERIFY(!store.updateStatus(QStringLiteral("nonexistent"), MsrpTransactionStatus::Accepted));
}

void TestMsrpTransactionStore::pendingCountExcludesTerminal()
{
    MsrpTransactionStore store;
    store.begin(QStringLiteral("t1"), QStringLiteral("s1"), MsrpMethod::Send, QStringLiteral("m1"), true);
    store.begin(QStringLiteral("t2"), QStringLiteral("s1"), MsrpMethod::Send, QStringLiteral("m2"), true);
    store.updateStatus(QStringLiteral("t1"), MsrpTransactionStatus::Accepted, 200);
    QCOMPARE(store.pendingCount(QStringLiteral("s1")), 1);
}

void TestMsrpTransactionStore::clearRemovesAll()
{
    MsrpTransactionStore store;
    store.begin(QStringLiteral("t1"), QStringLiteral("s1"), MsrpMethod::Send, QStringLiteral("m1"), true);
    store.clear();
    QVERIFY(store.all().isEmpty());
}

QTEST_GUILESS_MAIN(TestMsrpTransactionStore)
#include "test_msrp_transaction_store.moc"
