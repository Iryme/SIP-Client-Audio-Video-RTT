#include <QtTest/QtTest>

#include "msrp/MsrpChunkAssembler.h"
#include "msrp/MsrpMessageChunker.h"

class TestMsrpChunkAssembler : public QObject
{
    Q_OBJECT

private slots:
    void singleChunkCompletesImmediately();
    void multiChunkReassemblesInOrder();
    void duplicateChunkIgnored();
    void gapDetected();
    void overlapDetected();
    void abortRemovesPending();
    void limitExceededRejected();
    void purgeStaleRemovesOldEntries();
};

void TestMsrpChunkAssembler::singleChunkCompletesImmediately()
{
    MsrpChunkAssembler assembler;
    MsrpFrame f;
    f.messageId = QStringLiteral("m1");
    f.contentType = QStringLiteral("text/plain");
    f.body = "hello";
    f.continuation = MsrpContinuation::Complete;
    const auto r = assembler.feedChunk(f);
    QCOMPARE(r.status, MsrpChunkAssembler::FeedStatus::Complete);
    QCOMPARE(r.message.body, QByteArray("hello"));
}

void TestMsrpChunkAssembler::multiChunkReassemblesInOrder()
{
    const QByteArray body(2500, 'X');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);

    MsrpChunkAssembler assembler;
    MsrpChunkAssembler::FeedResult last;
    for (const auto &f : frames)
        last = assembler.feedChunk(f);

    QCOMPARE(last.status, MsrpChunkAssembler::FeedStatus::Complete);
    QCOMPARE(last.message.body, body);
}

void TestMsrpChunkAssembler::duplicateChunkIgnored()
{
    const QByteArray body(2500, 'Y');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);

    MsrpChunkAssembler assembler;
    assembler.feedChunk(frames.at(0));
    const auto dup = assembler.feedChunk(frames.at(0));
    QCOMPARE(dup.status, MsrpChunkAssembler::FeedStatus::InProgress);
    QVERIFY(!dup.message.warnings.isEmpty());
}

void TestMsrpChunkAssembler::gapDetected()
{
    const QByteArray body(2500, 'Z');
    const auto frames = MsrpMessageChunker::buildSendFrames(
        QStringLiteral("s"), QStringLiteral("to"), QStringLiteral("from"),
        QStringLiteral("m1"), QStringLiteral("text/plain"), body, 1000, false, false);

    MsrpChunkAssembler assembler;
    assembler.feedChunk(frames.at(0));
    const auto r = assembler.feedChunk(frames.at(2)); // skip chunk 1 -> gap
    QVERIFY(!r.message.warnings.isEmpty());
    QVERIFY(r.message.warnings.first().contains(QStringLiteral("gap"), Qt::CaseInsensitive));
}

void TestMsrpChunkAssembler::overlapDetected()
{
    MsrpChunkAssembler assembler;
    MsrpFrame f1;
    f1.messageId = QStringLiteral("m1");
    f1.hasByteRange = true;
    f1.byteRange = MsrpByteRange{1, 100, 200};
    f1.body = QByteArray(100, 'A');
    f1.continuation = MsrpContinuation::More;
    assembler.feedChunk(f1);

    MsrpFrame f2;
    f2.messageId = QStringLiteral("m1");
    f2.hasByteRange = true;
    f2.byteRange = MsrpByteRange{50, 200, 200}; // overlaps with f1
    f2.body = QByteArray(151, 'B');
    f2.continuation = MsrpContinuation::Complete;
    const auto r = assembler.feedChunk(f2);
    QVERIFY(!r.message.warnings.isEmpty());
}

void TestMsrpChunkAssembler::abortRemovesPending()
{
    MsrpChunkAssembler assembler;
    MsrpFrame f;
    f.messageId = QStringLiteral("m1");
    f.hasByteRange = true;
    f.byteRange = MsrpByteRange{1, 100, 200};
    f.body = QByteArray(100, 'A');
    f.continuation = MsrpContinuation::More;
    assembler.feedChunk(f);
    QVERIFY(assembler.hasPending(QStringLiteral("m1")));
    assembler.abort(QStringLiteral("m1"));
    QVERIFY(!assembler.hasPending(QStringLiteral("m1")));
}

void TestMsrpChunkAssembler::limitExceededRejected()
{
    MsrpChunkAssembler assembler(50); // tiny cap
    MsrpFrame f;
    f.messageId = QStringLiteral("m1");
    f.body = QByteArray(1000, 'A');
    f.continuation = MsrpContinuation::Complete;
    const auto r = assembler.feedChunk(f);
    QCOMPARE(r.status, MsrpChunkAssembler::FeedStatus::Error);
}

void TestMsrpChunkAssembler::purgeStaleRemovesOldEntries()
{
    MsrpChunkAssembler assembler;
    MsrpFrame f;
    f.messageId = QStringLiteral("m1");
    f.hasByteRange = true;
    f.byteRange = MsrpByteRange{1, 100, 200};
    f.body = QByteArray(100, 'A');
    f.continuation = MsrpContinuation::More;
    assembler.feedChunk(f);

    const auto purged = assembler.purgeStale(QDateTime::currentDateTimeUtc().addSecs(60));
    QCOMPARE(purged.size(), 1);
    QVERIFY(!assembler.hasPending(QStringLiteral("m1")));
}

QTEST_GUILESS_MAIN(TestMsrpChunkAssembler)
#include "test_msrp_chunk_assembler.moc"
