#include <QtTest/QtTest>

#include "msrp/MsrpFrameParser.h"
#include "msrp/MsrpFrameSerializer.h"

class TestMsrpFrameSerializer : public QObject
{
    Q_OBJECT

private slots:
    void serializesSendRequest();
    void serializesReportRequest();
    void serializesResponse();
    void roundTripsBinaryBody();
    void rejectsHeaderInjection();
    void producesExactCrlfDelimiter();
    void serializesByteRange();
};

void TestMsrpFrameSerializer::serializesSendRequest()
{
    MsrpFrame f;
    f.isRequest = true;
    f.transactionId = QStringLiteral("tid1");
    f.method = QStringLiteral("SEND");
    f.toPath = QStringLiteral("msrp://b:2855/y;tcp");
    f.fromPath = QStringLiteral("msrp://a:2855/x;tcp");
    f.messageId = QStringLiteral("m1");
    f.contentType = QStringLiteral("text/plain");
    f.body = "Hello!";
    f.continuation = MsrpContinuation::Complete;

    bool ok = false;
    const QByteArray out = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(ok);
    QVERIFY(out.startsWith("MSRP tid1 SEND\r\n"));
    QVERIFY(out.contains("To-Path: msrp://b:2855/y;tcp\r\n"));
    QVERIFY(out.contains("Content-Type: text/plain\r\n\r\nHello!"));
    QVERIFY(out.endsWith("\r\n-------tid1$\r\n"));
}

void TestMsrpFrameSerializer::serializesReportRequest()
{
    MsrpFrame f;
    f.isRequest = true;
    f.transactionId = QStringLiteral("r1");
    f.method = QStringLiteral("REPORT");
    f.status = QStringLiteral("000 200 OK");
    f.continuation = MsrpContinuation::Complete;

    bool ok = false;
    const QByteArray out = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(ok);
    QVERIFY(out.contains("MSRP r1 REPORT\r\n"));
    QVERIFY(out.contains("Status: 000 200 OK\r\n"));
}

void TestMsrpFrameSerializer::serializesResponse()
{
    MsrpFrame f;
    f.isRequest = false;
    f.transactionId = QStringLiteral("tid2");
    f.responseCode = 200;
    f.responseComment = QStringLiteral("OK");
    f.continuation = MsrpContinuation::Complete;

    bool ok = false;
    const QByteArray out = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(ok);
    QCOMPARE(out, QByteArray("MSRP tid2 200 OK\r\n\r\n-------tid2$\r\n"));
}

void TestMsrpFrameSerializer::roundTripsBinaryBody()
{
    MsrpFrame f;
    f.isRequest = true;
    f.transactionId = QStringLiteral("bin1");
    f.method = QStringLiteral("SEND");
    f.contentType = QStringLiteral("application/octet-stream");
    QByteArray body;
    for (int i = 0; i < 256; ++i)
        body.append(static_cast<char>(i));
    f.body = body;
    f.continuation = MsrpContinuation::Complete;

    bool ok = false;
    const QByteArray serialized = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(ok);

    MsrpFrameParser parser;
    const auto results = parser.feed(serialized);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().frame.body, body);
}

void TestMsrpFrameSerializer::rejectsHeaderInjection()
{
    MsrpFrame f;
    f.isRequest = true;
    f.transactionId = QStringLiteral("tid3");
    f.method = QStringLiteral("SEND");
    f.messageId = QStringLiteral("evil\r\nX-Injected: yes");

    bool ok = true;
    const QByteArray out = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(!ok);
    QVERIFY(out.isEmpty());
}

void TestMsrpFrameSerializer::producesExactCrlfDelimiter()
{
    MsrpFrame f;
    f.isRequest = true;
    f.transactionId = QStringLiteral("exact1");
    f.method = QStringLiteral("SEND");
    f.continuation = MsrpContinuation::More;

    bool ok = false;
    const QByteArray out = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(ok);
    QVERIFY(out.endsWith("\r\n-------exact1+\r\n"));
}

void TestMsrpFrameSerializer::serializesByteRange()
{
    MsrpFrame f;
    f.isRequest = true;
    f.transactionId = QStringLiteral("br1");
    f.method = QStringLiteral("SEND");
    f.hasByteRange = true;
    f.byteRange = MsrpByteRange{1, 2048, 8192};

    bool ok = false;
    const QByteArray out = MsrpFrameSerializer::serialize(f, &ok);
    QVERIFY(ok);
    QVERIFY(out.contains("Byte-Range: 1-2048/8192\r\n"));
}

QTEST_GUILESS_MAIN(TestMsrpFrameSerializer)
#include "test_msrp_frame_serializer.moc"
