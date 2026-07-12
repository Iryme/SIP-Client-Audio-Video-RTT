#include <QtTest/QtTest>

#include "msrp/MsrpFrameParser.h"

class TestMsrpFrameParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesCompleteSendRequest();
    void parsesCompleteReportRequest();
    void parses200Response();
    void handlesFragmentedHeaders();
    void handlesFragmentedBody();
    void handlesFragmentedDelimiter();
    void parsesMultipleFramesPerRead();
    void handlesBodyWithNulBytes();
    void handlesBodyWithHighBytes();
    void handlesCrlfInsideBody();
    void rejectsInvalidTransactionId();
    void toleratesUnknownHeaders();
    void toleratesDuplicateHeaders();
    void rejectsOversizedFrame();
    void recoversAfterInvalidFrame();
    void parsesByteRangeHeader();
    void parsesContinuationFlags();
};

namespace {
QByteArray sendFrame(const QByteArray &tid, const QByteArray &body, char flag = '$')
{
    QByteArray f;
    f += "MSRP " + tid + " SEND\r\n";
    f += "To-Path: msrp://bob.example.test:2855/b1;tcp\r\n";
    f += "From-Path: msrp://alice.example.test:2855/a1;tcp\r\n";
    f += "Message-ID: m1\r\n";
    f += "Content-Type: text/plain\r\n";
    f += "\r\n";
    f += body;
    f += "\r\n-------" + tid;
    f += flag;
    f += "\r\n";
    return f;
}
} // namespace

void TestMsrpFrameParser::parsesCompleteSendRequest()
{
    MsrpFrameParser parser;
    const auto results = parser.feed(sendFrame("tid1", "Hello!"));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().status, MsrpFrameParser::Status::Complete);
    const MsrpFrame &f = results.first().frame;
    QVERIFY(f.isRequest);
    QCOMPARE(f.method, QStringLiteral("SEND"));
    QCOMPARE(f.transactionId, QStringLiteral("tid1"));
    QCOMPARE(f.messageId, QStringLiteral("m1"));
    QCOMPARE(f.contentType, QStringLiteral("text/plain"));
    QCOMPARE(f.body, QByteArray("Hello!"));
    QCOMPARE(f.continuation, MsrpContinuation::Complete);
}

void TestMsrpFrameParser::parsesCompleteReportRequest()
{
    QByteArray f = "MSRP r1 REPORT\r\n";
    f += "To-Path: msrp://a:2855/x;tcp\r\n";
    f += "From-Path: msrp://b:2855/y;tcp\r\n";
    f += "Message-ID: m2\r\n";
    f += "Status: 000 200 OK\r\n";
    f += "\r\n-------r1$\r\n";

    MsrpFrameParser parser;
    const auto results = parser.feed(f);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.method, QStringLiteral("REPORT"));
    QCOMPARE(results.first().frame.status, QStringLiteral("000 200 OK"));
}

void TestMsrpFrameParser::parses200Response()
{
    QByteArray f = "MSRP tid1 200 OK\r\n\r\n-------tid1$\r\n";
    MsrpFrameParser parser;
    const auto results = parser.feed(f);
    QCOMPARE(results.size(), 1);
    QVERIFY(!results.first().frame.isRequest);
    QCOMPARE(results.first().frame.responseCode, 200);
    QVERIFY(results.first().frame.isSuccessResponse());
}

void TestMsrpFrameParser::handlesFragmentedHeaders()
{
    const QByteArray full = sendFrame("tid2", "Body");
    MsrpFrameParser parser;
    QList<MsrpFrameParser::Result> results;
    for (int i = 0; i < full.size(); i += 5)
        results += parser.feed(full.mid(i, 5));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.body, QByteArray("Body"));
}

void TestMsrpFrameParser::handlesFragmentedBody()
{
    const QByteArray full = sendFrame("tid3", "ABCDEFGHIJ");
    MsrpFrameParser parser;
    const int splitPoint = full.indexOf("ABCDE") + 5;
    auto r1 = parser.feed(full.left(splitPoint));
    QVERIFY(r1.isEmpty());
    auto r2 = parser.feed(full.mid(splitPoint));
    QCOMPARE(r2.size(), 1);
    QCOMPARE(r2.first().frame.body, QByteArray("ABCDEFGHIJ"));
}

void TestMsrpFrameParser::handlesFragmentedDelimiter()
{
    const QByteArray full = sendFrame("tid4", "X");
    MsrpFrameParser parser;
    const int splitPoint = full.size() - 3; // split inside the trailing CRLF/flag
    auto r1 = parser.feed(full.left(splitPoint));
    QVERIFY(r1.isEmpty());
    auto r2 = parser.feed(full.mid(splitPoint));
    QCOMPARE(r2.size(), 1);
    QCOMPARE(r2.first().status, MsrpFrameParser::Status::Complete);
}

void TestMsrpFrameParser::parsesMultipleFramesPerRead()
{
    const QByteArray combined = sendFrame("tidA", "one") + sendFrame("tidB", "two");
    MsrpFrameParser parser;
    const auto results = parser.feed(combined);
    QCOMPARE(results.size(), 2);
    QCOMPARE(results.at(0).frame.body, QByteArray("one"));
    QCOMPARE(results.at(1).frame.body, QByteArray("two"));
}

void TestMsrpFrameParser::handlesBodyWithNulBytes()
{
    QByteArray body("a\0b\0c", 5);
    MsrpFrameParser parser;
    const auto results = parser.feed(sendFrame("tid5", body));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.body, body);
}

void TestMsrpFrameParser::handlesBodyWithHighBytes()
{
    QByteArray body;
    for (int i = 0x80; i < 0x90; ++i)
        body.append(static_cast<char>(i));
    MsrpFrameParser parser;
    const auto results = parser.feed(sendFrame("tid6", body));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.body, body);
}

void TestMsrpFrameParser::handlesCrlfInsideBody()
{
    const QByteArray body = "line1\r\nline2\r\nline3";
    MsrpFrameParser parser;
    const auto results = parser.feed(sendFrame("tid7", body));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.body, body);
}

void TestMsrpFrameParser::rejectsInvalidTransactionId()
{
    QByteArray f = "MSRP bad!id SEND\r\n\r\n-------bad!id$\r\n";
    MsrpFrameParser parser;
    const auto results = parser.feed(f);
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().status, MsrpFrameParser::Status::Invalid);
}

void TestMsrpFrameParser::toleratesUnknownHeaders()
{
    QByteArray f = "MSRP tid8 SEND\r\n";
    f += "To-Path: msrp://a:2855/x;tcp\r\n";
    f += "From-Path: msrp://b:2855/y;tcp\r\n";
    f += "X-Custom-Header: value123\r\n";
    f += "\r\n-------tid8$\r\n";
    MsrpFrameParser parser;
    const auto results = parser.feed(f);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().status, MsrpFrameParser::Status::Complete);
    QCOMPARE(results.first().frame.unknownHeaders.value(QStringLiteral("X-Custom-Header")), QStringLiteral("value123"));
}

void TestMsrpFrameParser::toleratesDuplicateHeaders()
{
    QByteArray f = "MSRP tid9 SEND\r\n";
    f += "X-Dup: first\r\n";
    f += "X-Dup: second\r\n";
    f += "\r\n-------tid9$\r\n";
    MsrpFrameParser parser;
    const auto results = parser.feed(f);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.unknownHeaders.value(QStringLiteral("X-Dup")), QStringLiteral("second"));
}

void TestMsrpFrameParser::rejectsOversizedFrame()
{
    MsrpFrameParser parser(64); // tiny cap
    const QByteArray body(1000, 'A');
    const auto results = parser.feed(sendFrame("tidbig", body));
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().status, MsrpFrameParser::Status::LimitExceeded);
}

void TestMsrpFrameParser::recoversAfterInvalidFrame()
{
    QByteArray garbage = "MSRP !!! SEND\r\nnotdelimited";
    QByteArray good = sendFrame("tidgood", "recovered");
    MsrpFrameParser parser;
    const auto results = parser.feed(garbage + good);
    QVERIFY(results.size() >= 1);
    bool foundGood = false;
    for (const auto &r : results) {
        if (r.status == MsrpFrameParser::Status::Complete && r.frame.transactionId == QStringLiteral("tidgood"))
            foundGood = true;
    }
    QVERIFY(foundGood);
}

void TestMsrpFrameParser::parsesByteRangeHeader()
{
    QByteArray f = "MSRP tid10 SEND\r\n";
    f += "Byte-Range: 1-2048/8192\r\n";
    f += "\r\n-------tid10$\r\n";
    MsrpFrameParser parser;
    const auto results = parser.feed(f);
    QVERIFY(results.first().frame.hasByteRange);
    QCOMPARE(results.first().frame.byteRange.start, qint64(1));
    QCOMPARE(results.first().frame.byteRange.end, qint64(2048));
    QCOMPARE(results.first().frame.byteRange.total, qint64(8192));
}

void TestMsrpFrameParser::parsesContinuationFlags()
{
    MsrpFrameParser parser;
    auto r1 = parser.feed(sendFrame("t1", "a", '+'));
    QCOMPARE(r1.first().frame.continuation, MsrpContinuation::More);
    auto r2 = parser.feed(sendFrame("t2", "b", '#'));
    QCOMPARE(r2.first().frame.continuation, MsrpContinuation::Abort);
}

QTEST_GUILESS_MAIN(TestMsrpFrameParser)
#include "test_msrp_frame_parser.moc"
