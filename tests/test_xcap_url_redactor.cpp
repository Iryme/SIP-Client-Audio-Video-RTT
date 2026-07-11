#include <QtTest/QtTest>

#include "sip/XcapUrlRedactor.h"

class TestXcapUrlRedactor : public QObject
{
    Q_OBJECT

private slots:
    void keepsSchemeHostPortAndAuid();
    void dropsUserInfo();
    void dropsSensitiveQuery();
    void sameUrlRedactsDeterministically();
    void differentSelectorsRedactDifferently();
    void emptyUrlYieldsEmptyString();
    void malformedUrlNeverEchoesInput();
};

void TestXcapUrlRedactor::keepsSchemeHostPortAndAuid()
{
    const QString url = QStringLiteral(
        "https://xcap.example.test:8443/xcap-root/resource-lists/users/sip:alice@example.test/index");
    const QString redacted = XcapUrlRedactor::redact(url);
    QVERIFY(redacted.startsWith(QStringLiteral("https://xcap.example.test:8443")));
    QVERIFY(redacted.contains(QStringLiteral("xcap-root")));
    QVERIFY(redacted.contains(QStringLiteral("resource-lists")));
}

void TestXcapUrlRedactor::dropsUserInfo()
{
    const QString url = QStringLiteral("https://alice:secretpass@xcap.example.test/xcap-root/xcap-caps/global/index");
    const QString redacted = XcapUrlRedactor::redact(url);
    QVERIFY(!redacted.contains(QStringLiteral("secretpass")));
    QVERIFY(!redacted.contains(QStringLiteral("alice:")));
}

void TestXcapUrlRedactor::dropsSensitiveQuery()
{
    const QString url = QStringLiteral(
        "https://xcap.example.test/xcap-root/resource-lists/users/sip:alice@example.test/index?auth_token=abc123secret");
    const QString redacted = XcapUrlRedactor::redact(url);
    QVERIFY(!redacted.contains(QStringLiteral("abc123secret")));
    QVERIFY(!redacted.contains(QStringLiteral("auth_token")));
}

void TestXcapUrlRedactor::sameUrlRedactsDeterministically()
{
    const QString url = QStringLiteral(
        "https://xcap.example.test/xcap-root/resource-lists/users/sip:alice@example.test/index~~/resource-lists/list%5B@name=%22friends%22%5D");
    QCOMPARE(XcapUrlRedactor::redact(url), XcapUrlRedactor::redact(url));
}

void TestXcapUrlRedactor::differentSelectorsRedactDifferently()
{
    const QString urlA = QStringLiteral("https://xcap.example.test/xcap-root/resource-lists/users/sip:alice@example.test/index");
    const QString urlB = QStringLiteral("https://xcap.example.test/xcap-root/resource-lists/users/sip:bob@example.test/index");
    QVERIFY(XcapUrlRedactor::redact(urlA) != XcapUrlRedactor::redact(urlB));
}

void TestXcapUrlRedactor::emptyUrlYieldsEmptyString()
{
    QVERIFY(XcapUrlRedactor::redact(QString()).isEmpty());
}

void TestXcapUrlRedactor::malformedUrlNeverEchoesInput()
{
    const QString notAUrl = QStringLiteral("this-has-a-secret-token=abc123 but is not a URL");
    const QString redacted = XcapUrlRedactor::redact(notAUrl);
    QVERIFY(!redacted.contains(QStringLiteral("secret-token")));
}

QTEST_GUILESS_MAIN(TestXcapUrlRedactor)
#include "test_xcap_url_redactor.moc"
