#include <QtTest/QtTest>

#include "sip/XcapXmlValidator.h"

class TestXcapXmlValidator : public QObject
{
    Q_OBJECT

private slots:
    void acceptsWellFormedXml();
    void rejectsInvalidXml();
    void rejectsEmptyWhenNotAllowed();
    void allowsEmptyWhenAllowed();
    void rejectsDoctype();
    void rejectsUnknownEncoding();
    void acceptsKnownEncoding();
    void rejectsUnbalancedElements();
};

void TestXcapXmlValidator::acceptsWellFormedXml()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\">"
        "<list name=\"friends\"/></resource-lists>");
    const auto result = XcapXmlValidator::validate(xml, false);
    QVERIFY(result.ok);
    QVERIFY(result.wellFormed);
    QVERIFY(!result.empty);
}

void TestXcapXmlValidator::rejectsInvalidXml()
{
    const QString xml = QStringLiteral("<resource-lists><list name=\"friends\"></resource-lists>");
    const auto result = XcapXmlValidator::validate(xml, false);
    QVERIFY(!result.ok);
    QVERIFY(!result.errorMessage.isEmpty());
}

void TestXcapXmlValidator::rejectsEmptyWhenNotAllowed()
{
    const auto result = XcapXmlValidator::validate(QString(), false);
    QVERIFY(!result.ok);
    QVERIFY(result.empty);
}

void TestXcapXmlValidator::allowsEmptyWhenAllowed()
{
    const auto result = XcapXmlValidator::validate(QStringLiteral("   "), true);
    QVERIFY(result.ok);
    QVERIFY(result.empty);
}

void TestXcapXmlValidator::rejectsDoctype()
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<!DOCTYPE resource-lists [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]>"
        "<resource-lists>&xxe;</resource-lists>");
    const auto result = XcapXmlValidator::validate(xml, false);
    QVERIFY(!result.ok);
    QVERIFY(result.errorMessage.contains(QStringLiteral("DOCTYPE"), Qt::CaseInsensitive));
}

void TestXcapXmlValidator::rejectsUnknownEncoding()
{
    const QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"shift-jis\"?><a/>");
    const auto result = XcapXmlValidator::validate(xml, false);
    QVERIFY(!result.ok);
    QVERIFY(result.errorMessage.contains(QStringLiteral("encoding"), Qt::CaseInsensitive));
}

void TestXcapXmlValidator::acceptsKnownEncoding()
{
    const QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?><a/>");
    const auto result = XcapXmlValidator::validate(xml, false);
    QVERIFY(result.ok);
}

void TestXcapXmlValidator::rejectsUnbalancedElements()
{
    const QString xml = QStringLiteral("<a><b></a></b>");
    const auto result = XcapXmlValidator::validate(xml, false);
    QVERIFY(!result.ok);
}

QTEST_GUILESS_MAIN(TestXcapXmlValidator)
#include "test_xcap_xml_validator.moc"
