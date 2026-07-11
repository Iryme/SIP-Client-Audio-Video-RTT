#include <QtTest/QtTest>

#include "sip/XcapAuthHeaderBuilder.h"

class TestXcapAuthHeaderBuilder : public QObject
{
    Q_OBJECT

private slots:
    void buildsBasicAuthorizationHeader();
    void neverEchoesPasswordInPlaintext();
};

void TestXcapAuthHeaderBuilder::buildsBasicAuthorizationHeader()
{
    const QByteArray header = XcapAuthHeaderBuilder::basicAuthorizationHeader(
        QStringLiteral("alice"), QStringLiteral("s3cr3t"));
    // "alice:s3cr3t" base64-encoded.
    QCOMPARE(header, QByteArray("Basic YWxpY2U6czNjcjN0"));
}

void TestXcapAuthHeaderBuilder::neverEchoesPasswordInPlaintext()
{
    const QByteArray header = XcapAuthHeaderBuilder::basicAuthorizationHeader(
        QStringLiteral("bob"), QStringLiteral("plaintext-secret"));
    QVERIFY(!header.contains("plaintext-secret"));
}

QTEST_GUILESS_MAIN(TestXcapAuthHeaderBuilder)
#include "test_xcap_auth_header_builder.moc"
