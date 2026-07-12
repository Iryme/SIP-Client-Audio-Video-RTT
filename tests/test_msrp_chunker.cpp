#include <QtTest/QtTest>

#include "msrp/MsrpMessageChunker.h"

class TestMsrpChunker : public QObject
{
    Q_OBJECT

private slots:
    void singleChunkWhenUnderLimit();
    void splitsIntoMultipleChunks();
    void byteRangeCoversWholeMessage();
    void finalChunkMarkedComplete();
    void intermediateChunksMarkedMore();
    void reportHeadersOnlyOnFinalChunk();
    void eachChunkHasUniqueTransactionId();
};

void TestMsrpChunker::singleChunkWhenUnderLimit()
{
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), QByteArray("hello"), 2048, true, true);
    QCOMPARE(frames.size(), 1);
    QVERIFY(!frames.first().hasByteRange);
    QCOMPARE(frames.first().continuation, MsrpContinuation::Complete);
}

void TestMsrpChunker::splitsIntoMultipleChunks()
{
    const QByteArray body(2500, 'A');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, true, true);
    QCOMPARE(frames.size(), 3);
    QCOMPARE(frames.at(0).body.size(), 1000);
    QCOMPARE(frames.at(1).body.size(), 1000);
    QCOMPARE(frames.at(2).body.size(), 500);
}

void TestMsrpChunker::byteRangeCoversWholeMessage()
{
    const QByteArray body(2500, 'B');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);
    QCOMPARE(frames.at(0).byteRange.start, qint64(1));
    QCOMPARE(frames.at(0).byteRange.end, qint64(1000));
    QCOMPARE(frames.at(0).byteRange.total, qint64(2500));
    QCOMPARE(frames.at(1).byteRange.start, qint64(1001));
    QCOMPARE(frames.at(1).byteRange.end, qint64(2000));
    QCOMPARE(frames.at(2).byteRange.start, qint64(2001));
    QCOMPARE(frames.at(2).byteRange.end, qint64(2500));
}

void TestMsrpChunker::finalChunkMarkedComplete()
{
    const QByteArray body(2500, 'C');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);
    QCOMPARE(frames.last().continuation, MsrpContinuation::Complete);
}

void TestMsrpChunker::intermediateChunksMarkedMore()
{
    const QByteArray body(2500, 'D');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);
    QCOMPARE(frames.at(0).continuation, MsrpContinuation::More);
    QCOMPARE(frames.at(1).continuation, MsrpContinuation::More);
}

void TestMsrpChunker::reportHeadersOnlyOnFinalChunk()
{
    const QByteArray body(2500, 'E');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, true, true);
    QCOMPARE(frames.at(0).successReport, QStringLiteral("no"));
    QCOMPARE(frames.at(1).successReport, QStringLiteral("no"));
    QCOMPARE(frames.last().successReport, QStringLiteral("yes"));
}

void TestMsrpChunker::eachChunkHasUniqueTransactionId()
{
    const QByteArray body(2500, 'F');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);
    QVERIFY(frames.at(0).transactionId != frames.at(1).transactionId);
    QVERIFY(frames.at(1).transactionId != frames.at(2).transactionId);
}

QTEST_GUILESS_MAIN(TestMsrpChunker)
#include "test_msrp_chunker.moc"
