#include <QtTest/QtTest>

#include "sip/SipBodyExtractor.h"

class TestSipBodyExtractor : public QObject
{
    Q_OBJECT

private slots:
    void contentLengthByteAccurate();
    void fallsBackToRestOfBodyWhenNoContentLength();
    void bodyWithNullBytesIsPreserved();
    void noSeparatorYieldsNoBody();
    void extraBytesAfterContentLengthAreIgnored();
};

void TestSipBodyExtractor::contentLengthByteAccurate()
{
    // Body is declared as 5 bytes via Content-Length, but the wire capture
    // (as can genuinely happen with some senders/proxies) has trailing
    // garbage after it — the extractor must return exactly 5 bytes, not
    // "everything after the separator".
    const QString rawSip = QStringLiteral(
        "MESSAGE sip:bob@example.com SIP/2.0\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "HelloTRAILING-GARBAGE-NOT-PART-OF-BODY");

    const SipBodyExtractor::Result r = SipBodyExtractor::extract(rawSip);
    QVERIFY(r.headerFound);
    QVERIFY(r.contentLengthUsed);
    QCOMPARE(r.rawBodyBytes, QByteArray("Hello"));
}

void TestSipBodyExtractor::fallsBackToRestOfBodyWhenNoContentLength()
{
    const QString rawSip = QStringLiteral(
        "MESSAGE sip:bob@example.com SIP/2.0\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hi Bob!");

    const SipBodyExtractor::Result r = SipBodyExtractor::extract(rawSip);
    QVERIFY(r.headerFound);
    QVERIFY(!r.contentLengthUsed);
    QCOMPARE(r.rawBodyBytes, QByteArray("Hi Bob!"));
}

void TestSipBodyExtractor::bodyWithNullBytesIsPreserved()
{
    QByteArray body;
    body.append("before");
    body.append('\0');
    body.append("after");

    QString rawSip = QStringLiteral(
        "MESSAGE sip:bob@example.com SIP/2.0\r\n"
        "Content-Length: 12\r\n"
        "\r\n");
    rawSip += QString::fromLatin1(body); // Latin-1 round trip preserves every byte, incl. NUL

    const SipBodyExtractor::Result r = SipBodyExtractor::extract(rawSip);
    QVERIFY(r.contentLengthUsed);
    QCOMPARE(r.rawBodyBytes.size(), 12);
    QCOMPARE(r.rawBodyBytes, body);
}

void TestSipBodyExtractor::noSeparatorYieldsNoBody()
{
    const QString rawSip = QStringLiteral("MESSAGE sip:bob@example.com SIP/2.0\r\nContent-Type: text/plain");
    const SipBodyExtractor::Result r = SipBodyExtractor::extract(rawSip);
    QVERIFY(!r.headerFound);
    QVERIFY(r.rawBodyBytes.isEmpty());
}

void TestSipBodyExtractor::extraBytesAfterContentLengthAreIgnored()
{
    // A negative/unusable Content-Length must not crash or misbehave —
    // falls back to "everything after the separator".
    const QString rawSip = QStringLiteral(
        "MESSAGE sip:bob@example.com SIP/2.0\r\n"
        "Content-Length: not-a-number\r\n"
        "\r\n"
        "Body text");

    const SipBodyExtractor::Result r = SipBodyExtractor::extract(rawSip);
    QVERIFY(!r.contentLengthUsed);
    QCOMPARE(r.rawBodyBytes, QByteArray("Body text"));
}

QTEST_GUILESS_MAIN(TestSipBodyExtractor)
#include "test_sip_body_extractor.moc"
