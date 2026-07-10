#include <QtTest/QtTest>

#include "sip/UrlRedactor.h"

class TestUrlRedactor : public QObject
{
    Q_OBJECT

private slots:
    void dropsQueryToken();
    void keepsSchemeHostPort();
    void sameUrlRedactsDeterministically();
    void differentTokensRedactToDifferentValues();
    void emptyUrlYieldsEmptyString();
    void malformedUrlNeverEchoesInput();
};

void TestUrlRedactor::dropsQueryToken()
{
    const QString url = QStringLiteral("https://files.example.test/dl/abc123?token=super-secret-one-time-token");
    const QString redacted = UrlRedactor::redact(url);
    QVERIFY(!redacted.contains(QStringLiteral("super-secret-one-time-token")));
    QVERIFY(!redacted.contains(QStringLiteral("token=")));
}

void TestUrlRedactor::keepsSchemeHostPort()
{
    const QString url = QStringLiteral("https://files.example.test:8443/dl/abc123?token=xyz");
    const QString redacted = UrlRedactor::redact(url);
    QVERIFY(redacted.startsWith(QStringLiteral("https://files.example.test:8443")));
}

void TestUrlRedactor::sameUrlRedactsDeterministically()
{
    const QString url = QStringLiteral("https://files.example.test/dl/abc123/def456?token=xyz");
    QCOMPARE(UrlRedactor::redact(url), UrlRedactor::redact(url));
}

void TestUrlRedactor::differentTokensRedactToDifferentValues()
{
    const QString urlA = QStringLiteral("https://files.example.test/dl/abc123?token=aaa");
    const QString urlB = QStringLiteral("https://files.example.test/dl/abc123?token=bbb");
    // Query strings differ; the redacted fingerprint should too.
    QVERIFY(UrlRedactor::redact(urlA) != UrlRedactor::redact(urlB));
}

void TestUrlRedactor::emptyUrlYieldsEmptyString()
{
    QVERIFY(UrlRedactor::redact(QString()).isEmpty());
}

void TestUrlRedactor::malformedUrlNeverEchoesInput()
{
    const QString notAUrl = QStringLiteral("this-has-a-secret-token=abc123 but is not a URL");
    const QString redacted = UrlRedactor::redact(notAUrl);
    QVERIFY(!redacted.contains(QStringLiteral("secret-token")));
}

QTEST_GUILESS_MAIN(TestUrlRedactor)
#include "test_url_redactor.moc"
