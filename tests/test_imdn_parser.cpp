#include <QtTest/QtTest>

#include "sip/ImdnParser.h"

// Tests for ImdnParser (RFC 5438 message/imdn+xml) — pure text/XML parsing,
// no PJSIP/network needed.

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

QTEST_GUILESS_MAIN(TestImdnParser)
#include "test_imdn_parser.moc"
