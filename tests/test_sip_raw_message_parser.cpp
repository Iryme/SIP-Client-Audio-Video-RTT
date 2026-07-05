#include <QtTest/QtTest>

#include "sip/SipRawMessageParser.h"

// Tests for SipRawMessageParser — pure text parsing, no PJSIP/network needed.

class TestSipRawMessageParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesInviteWithSdpBody();
    void parsesStatusLineResponse();
    void parsesMethodFromCSeqWhenNoRequestLine();
};

void TestSipRawMessageParser::parsesInviteWithSdpBody()
{
    const QString raw =
        QStringLiteral("INVITE sip:bob@example.com SIP/2.0\r\n"
                       "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds\r\n"
                       "From: Alice <sip:alice@example.com>;tag=1928301774\r\n"
                       "To: Bob <sip:bob@example.com>\r\n"
                       "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n"
                       "CSeq: 314159 INVITE\r\n"
                       "Content-Type: application/sdp\r\n"
                       "Content-Length: 42\r\n"
                       "\r\n"
                       "v=0\r\n"
                       "m=audio 49170 RTP/AVP 0\r\n"
                       "m=text 49172 RTP/AVP 98\r\n");

    const SipMessageTrace t = SipRawMessageParser::parse(
        raw, SipMessageTrace::Direction::Outbound);

    QCOMPARE(t.method, QStringLiteral("INVITE"));
    QCOMPARE(t.statusCode, 0);
    QCOMPARE(t.callId, QStringLiteral("a84b4c76e66710@pc33.atlanta.com"));
    QCOMPARE(t.cSeq, QStringLiteral("314159 INVITE"));
    QCOMPARE(t.contentType, QStringLiteral("application/sdp"));
    QVERIFY(t.fromUri.contains(QStringLiteral("alice@example.com")));
    QVERIFY(t.toUri.contains(QStringLiteral("bob@example.com")));
    QCOMPARE(t.rawSip, raw);
}

void TestSipRawMessageParser::parsesStatusLineResponse()
{
    const QString raw =
        QStringLiteral("SIP/2.0 200 OK\r\n"
                       "Call-ID: a84b4c76e66710@pc33.atlanta.com\r\n"
                       "CSeq: 314159 INVITE\r\n"
                       "Content-Length: 0\r\n\r\n");

    const SipMessageTrace t = SipRawMessageParser::parse(
        raw, SipMessageTrace::Direction::Inbound);

    QCOMPARE(t.statusCode, 200);
    QCOMPARE(t.statusText, QStringLiteral("OK"));
    QCOMPARE(t.method, QStringLiteral("INVITE")); // recovered from CSeq
    QCOMPARE(t.direction, SipMessageTrace::Direction::Inbound);
}

void TestSipRawMessageParser::parsesMethodFromCSeqWhenNoRequestLine()
{
    const SipMessageTrace t = SipRawMessageParser::parse(
        QStringLiteral("SIP/2.0 180 Ringing\r\nCSeq: 1 INVITE\r\n\r\n"),
        SipMessageTrace::Direction::Inbound);

    QCOMPARE(t.statusCode, 180);
    QCOMPARE(t.statusText, QStringLiteral("Ringing"));
    QCOMPARE(t.method, QStringLiteral("INVITE"));
}

QTEST_GUILESS_MAIN(TestSipRawMessageParser)
#include "test_sip_raw_message_parser.moc"
