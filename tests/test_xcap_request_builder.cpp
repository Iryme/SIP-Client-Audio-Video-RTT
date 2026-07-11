#include <QtTest/QtTest>

#include "sip/XcapRequestBuilder.h"

class TestXcapRequestBuilder : public QObject
{
    Q_OBJECT

private slots:
    void buildsGetRequestModel();
    void buildsPutRequestModelWithContentType();
    void buildsDeleteRequestModel();
    void buildsHeadRequestModel();
    void appliesTimeoutFromConfig();
    void globalDocumentOmitsUsersSegment();
    void perDocumentXuiOverridesServerXui();
    void appendsNodeSelector();
};

namespace {
XcapServerConfig makeConfig()
{
    XcapServerConfig config;
    config.rootUri = QStringLiteral("https://xcap.example.test/xcap-root");
    config.xui = QStringLiteral("sip:alice@example.test");
    config.timeoutSeconds = 15;
    config.verifyTls = true;
    return config;
}

XcapDocument makeDocument()
{
    XcapDocument doc;
    doc.auid = QStringLiteral("resource-lists");
    doc.documentName = QStringLiteral("index");
    return doc;
}
} // namespace

void TestXcapRequestBuilder::buildsGetRequestModel()
{
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Get, makeConfig(), makeDocument());
    QCOMPARE(req.method, QStringLiteral("GET"));
    QCOMPARE(req.url.toString(), QStringLiteral("https://xcap.example.test/xcap-root/resource-lists/users/sip:alice@example.test/index"));
    QVERIFY(req.contentType.isEmpty());
}

void TestXcapRequestBuilder::buildsPutRequestModelWithContentType()
{
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Put, makeConfig(), makeDocument());
    QCOMPARE(req.method, QStringLiteral("PUT"));
    QCOMPARE(req.contentType, QStringLiteral("application/xml"));
}

void TestXcapRequestBuilder::buildsDeleteRequestModel()
{
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Delete, makeConfig(), makeDocument());
    QCOMPARE(req.method, QStringLiteral("DELETE"));
    QVERIFY(req.contentType.isEmpty());
}

void TestXcapRequestBuilder::buildsHeadRequestModel()
{
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Head, makeConfig(), makeDocument());
    QCOMPARE(req.method, QStringLiteral("HEAD"));
}

void TestXcapRequestBuilder::appliesTimeoutFromConfig()
{
    XcapServerConfig config = makeConfig();
    config.timeoutSeconds = 42;
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Get, config, makeDocument());
    QCOMPARE(req.timeoutMs, 42000);
}

void TestXcapRequestBuilder::globalDocumentOmitsUsersSegment()
{
    XcapDocument doc = makeDocument();
    doc.auid = QStringLiteral("xcap-caps");
    doc.xui.clear();
    XcapServerConfig config = makeConfig();
    config.xui.clear();
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Get, config, doc);
    QVERIFY(req.url.toString().contains(QStringLiteral("/xcap-caps/global/index")));
}

void TestXcapRequestBuilder::perDocumentXuiOverridesServerXui()
{
    XcapDocument doc = makeDocument();
    doc.xui = QStringLiteral("sip:bob@example.test");
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Get, makeConfig(), doc);
    QVERIFY(req.url.toString().contains(QStringLiteral("sip:bob@example.test")));
    QVERIFY(!req.url.toString().contains(QStringLiteral("sip:alice@example.test")));
}

void TestXcapRequestBuilder::appendsNodeSelector()
{
    XcapDocument doc = makeDocument();
    doc.nodeSelector = QStringLiteral("/resource-lists/list%5B@name=%22friends%22%5D");
    const auto req = XcapRequestBuilder::build(XcapHttpMethod::Get, makeConfig(), doc);
    QVERIFY(req.url.toString().contains(QStringLiteral("~~")));
}

QTEST_GUILESS_MAIN(TestXcapRequestBuilder)
#include "test_xcap_request_builder.moc"
