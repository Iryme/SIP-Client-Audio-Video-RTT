#include <QtTest/QtTest>

#include "sip/CpimParser.h"

// Tests for CpimParser (RFC 3862 CPIM header wrapper) — pure text parsing,
// no PJSIP/network needed.

class TestCpimParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesFullCpimHeaderBlock();
    void wrappedBodyIsExtracted();
    void emptyBodyIsNotPresent();
    void garbledBodyWithoutHeadersIsNotPresent();
};

void TestCpimParser::parsesFullCpimHeaderBlock()
{
    const QString body =
        QStringLiteral("From: MR SANDERS <im:piglet@100acre.com>\r\n"
                       "To: Depressed Donkey <im:eeyore@100acre.com>\r\n"
                       "DateTime: 2000-12-13T13:40:00-08:00\r\n"
                       "Subject: the weather will be fine today\r\n"
                       "Content-Type: text/plain; charset=utf-8\r\n"
                       "\r\n"
                       "Wheee!\r\n");

    const CpimInfo info = CpimParser::parse(body);

    QVERIFY(info.present);
    QVERIFY(info.from.contains(QStringLiteral("piglet@100acre.com")));
    QVERIFY(info.to.contains(QStringLiteral("eeyore@100acre.com")));
    QCOMPARE(info.dateTime, QStringLiteral("2000-12-13T13:40:00-08:00"));
    QCOMPARE(info.subject, QStringLiteral("the weather will be fine today"));
    QCOMPARE(info.contentType, QStringLiteral("text/plain; charset=utf-8"));
}

void TestCpimParser::wrappedBodyIsExtracted()
{
    const QString body =
        QStringLiteral("From: <im:alice@example.com>\r\n"
                       "To: <im:bob@example.com>\r\n"
                       "Content-Type: text/plain\r\n"
                       "\r\n"
                       "Hello there!");

    const CpimInfo info = CpimParser::parse(body);

    QVERIFY(info.present);
    QCOMPARE(info.wrappedBody, QStringLiteral("Hello there!"));
}

void TestCpimParser::emptyBodyIsNotPresent()
{
    const CpimInfo info = CpimParser::parse(QString());
    QVERIFY(!info.present);
}

void TestCpimParser::garbledBodyWithoutHeadersIsNotPresent()
{
    const CpimInfo info = CpimParser::parse(QStringLiteral("not a cpim message at all"));
    QVERIFY(!info.present);
}

QTEST_GUILESS_MAIN(TestCpimParser)
#include "test_cpim_parser.moc"
