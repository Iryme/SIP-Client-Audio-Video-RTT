#include <QtTest/QtTest>

#include "sip/ImdnGenerator.h"
#include "sip/ImdnParser.h"

// Tests for ImdnParser (RFC 5438 message/imdn+xml) — pure text/XML parsing,
// no PJSIP/network needed. Also covers ImdnGenerator (Task W096) and its
// round-trip through the same parser.

class TestImdnParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesDeliveredNotification();
    void parsesDisplayedNotification();
    void parsesFailedNotification();
    void parsesRecipients();
    void emptyBodyIsNotPresent();
    void nonImdnXmlIsNotPresent();
    void parsesForbiddenNotification();
    void parsesProcessedNotification();

    // Task W096: ImdnGenerator
    void generatesDeliveredAndRoundTrips();
    void generatesDisplayedAndRoundTrips();
    void generatesFailedAndRoundTrips();
    void generatesErrorAndRoundTrips();
    void generatorRejectsEmptyMessageId();
    void generatorRejectsNoneDisposition();
    void generatorEscapesUtf8AndSpecialChars();
};

void TestImdnParser::parsesDeliveredNotification()
{
    const QString xml =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n"
                       "  <message-id>34jk324j</message-id>\n"
                       "  <datetime>2008-04-04T12:16:49-05:00</datetime>\n"
                       "  <delivery-notification>\n"
                       "    <status><delivered/></status>\n"
                       "  </delivery-notification>\n"
                       "</imdn>\n");

    const ImdnInfo info = ImdnParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Delivered);
    QCOMPARE(info.messageId, QStringLiteral("34jk324j"));
}

void TestImdnParser::parsesDisplayedNotification()
{
    const QString xml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n"
                       "  <message-id>abc-1</message-id>\n"
                       "  <display-notification>\n"
                       "    <status><displayed/></status>\n"
                       "  </display-notification>\n"
                       "</imdn>\n");

    const ImdnInfo info = ImdnParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Displayed);
}

void TestImdnParser::parsesFailedNotification()
{
    const QString xml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n"
                       "  <message-id>abc-2</message-id>\n"
                       "  <delivery-notification>\n"
                       "    <status><failed/></status>\n"
                       "  </delivery-notification>\n"
                       "</imdn>\n");

    const ImdnInfo info = ImdnParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Failed);
}

void TestImdnParser::parsesRecipients()
{
    const QString xml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n"
                       "  <message-id>abc-3</message-id>\n"
                       "  <original-recipient>sip:alice@example.com</original-recipient>\n"
                       "  <final-recipient>sip:bob@example.com</final-recipient>\n"
                       "  <delivery-notification>\n"
                       "    <status><delivered/></status>\n"
                       "  </delivery-notification>\n"
                       "</imdn>\n");

    const ImdnInfo info = ImdnParser::parse(xml);

    QVERIFY(info.present);
    QCOMPARE(info.originalRecipient, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(info.finalRecipient, QStringLiteral("sip:bob@example.com"));
}

void TestImdnParser::emptyBodyIsNotPresent()
{
    const ImdnInfo info = ImdnParser::parse(QString());
    QVERIFY(!info.present);
}

void TestImdnParser::nonImdnXmlIsNotPresent()
{
    const ImdnInfo info = ImdnParser::parse(QStringLiteral("<not-imdn><foo>bar</foo></not-imdn>"));
    QVERIFY(!info.present);
}

void TestImdnParser::parsesForbiddenNotification()
{
    const QString xml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n"
                       "  <message-id>abc-4</message-id>\n"
                       "  <delivery-notification>\n"
                       "    <status><forbidden/></status>\n"
                       "  </delivery-notification>\n"
                       "</imdn>\n");
    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Forbidden);
}

void TestImdnParser::parsesProcessedNotification()
{
    const QString xml =
        QStringLiteral("<imdn xmlns=\"urn:ietf:params:xml:ns:imdn\">\n"
                       "  <message-id>abc-5</message-id>\n"
                       "  <display-notification>\n"
                       "    <status><processed/></status>\n"
                       "  </display-notification>\n"
                       "</imdn>\n");
    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Processed);
}

void TestImdnParser::generatesDeliveredAndRoundTrips()
{
    const QString xml = ImdnGenerator::generate(QStringLiteral("msg-1"),
                                                ImdnInfo::Disposition::Delivered);
    QVERIFY(!xml.isEmpty());
    QVERIFY(xml.contains(QStringLiteral("<delivered/>")));

    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Delivered);
    QCOMPARE(info.messageId, QStringLiteral("msg-1"));
}

void TestImdnParser::generatesDisplayedAndRoundTrips()
{
    const QString xml = ImdnGenerator::generate(QStringLiteral("msg-2"),
                                                ImdnInfo::Disposition::Displayed,
                                                QStringLiteral("sip:alice@example.com"),
                                                QStringLiteral("sip:bob@example.com"));
    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Displayed);
    QCOMPARE(info.messageId, QStringLiteral("msg-2"));
    QCOMPARE(info.originalRecipient, QStringLiteral("sip:alice@example.com"));
    QCOMPARE(info.finalRecipient, QStringLiteral("sip:bob@example.com"));
}

void TestImdnParser::generatesFailedAndRoundTrips()
{
    const QString xml = ImdnGenerator::generate(QStringLiteral("msg-3"),
                                                ImdnInfo::Disposition::Failed);
    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Failed);
}

void TestImdnParser::generatesErrorAndRoundTrips()
{
    const QString xml = ImdnGenerator::generate(QStringLiteral("msg-4"),
                                                ImdnInfo::Disposition::Error);
    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.disposition, ImdnInfo::Disposition::Error);
}

void TestImdnParser::generatorRejectsEmptyMessageId()
{
    const QString xml = ImdnGenerator::generate(QString(), ImdnInfo::Disposition::Delivered);
    QVERIFY(xml.isEmpty());
}

void TestImdnParser::generatorRejectsNoneDisposition()
{
    const QString xml = ImdnGenerator::generate(QStringLiteral("msg-5"),
                                                ImdnInfo::Disposition::None);
    QVERIFY(xml.isEmpty());
}

void TestImdnParser::generatorEscapesUtf8AndSpecialChars()
{
    const QString messageId = QString::fromUtf8("msg-<6>-&-\xc4\x83"); // includes '<','&' and UTF-8 letter (ă)
    const QString xml = ImdnGenerator::generate(messageId, ImdnInfo::Disposition::Delivered);
    QVERIFY(!xml.contains(QStringLiteral("<6>"))); // raw '<' must have been escaped

    const ImdnInfo info = ImdnParser::parse(xml);
    QVERIFY(info.present);
    QCOMPARE(info.messageId, messageId);
}

QTEST_GUILESS_MAIN(TestImdnParser)
#include "test_imdn_parser.moc"
