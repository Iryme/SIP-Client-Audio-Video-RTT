#include <QtTest/QtTest>
#include "sip/SipUriNormalizer.h"

class TestSipUriNormalizer : public QObject
{
    Q_OBJECT

private slots:
    // Already-correct SIP URIs
    void fullSipUri_passThrough();
    void fullSipsUri_passThrough();
    void sipUriCaseInsensitive_passThrough();

    // URIs that need "sip:" prepended
    void userAtDomain_prependsSip();
    void userAtDomainWithPort_prependsSip();

    // Extension-only with fallback domain
    void extensionWithFallbackDomain_qualifies();
    void extensionWithFallbackDomain_numeric();

    // Invalid inputs
    void emptyString_invalid();
    void whitespaceOnly_invalid();
    void noAtNoDomain_invalid();
    void atSignAtStart_invalid();
    void atSignAtEnd_invalid();
    void containsSpace_invalid();
};

using R = SipUriNormalizer::Result;

void TestSipUriNormalizer::fullSipUri_passThrough()
{
    const R r = SipUriNormalizer::normalize("sip:1002@sensor-x.local");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("sip:1002@sensor-x.local"));
}

void TestSipUriNormalizer::fullSipsUri_passThrough()
{
    const R r = SipUriNormalizer::normalize("sips:alice@example.com");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("sips:alice@example.com"));
}

void TestSipUriNormalizer::sipUriCaseInsensitive_passThrough()
{
    const R r = SipUriNormalizer::normalize("SIP:bob@example.com");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("SIP:bob@example.com"));
}

void TestSipUriNormalizer::userAtDomain_prependsSip()
{
    const R r = SipUriNormalizer::normalize("1002@sensor-x.local");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("sip:1002@sensor-x.local"));
}

void TestSipUriNormalizer::userAtDomainWithPort_prependsSip()
{
    const R r = SipUriNormalizer::normalize("alice@192.168.1.1:5060");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("sip:alice@192.168.1.1:5060"));
}

void TestSipUriNormalizer::extensionWithFallbackDomain_qualifies()
{
    const R r = SipUriNormalizer::normalize("1002", "sensor-x.local");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("sip:1002@sensor-x.local"));
}

void TestSipUriNormalizer::extensionWithFallbackDomain_numeric()
{
    const R r = SipUriNormalizer::normalize("paul", "sensor-x.local");
    QVERIFY(r.isValid);
    QCOMPARE(r.uri, QStringLiteral("sip:paul@sensor-x.local"));
}

void TestSipUriNormalizer::emptyString_invalid()
{
    const R r = SipUriNormalizer::normalize("");
    QVERIFY(!r.isValid);
    QVERIFY(!r.error.isEmpty());
}

void TestSipUriNormalizer::whitespaceOnly_invalid()
{
    const R r = SipUriNormalizer::normalize("   ");
    QVERIFY(!r.isValid);
    QVERIFY(!r.error.isEmpty());
}

void TestSipUriNormalizer::noAtNoDomain_invalid()
{
    const R r = SipUriNormalizer::normalize("justextension");
    QVERIFY(!r.isValid);
    QVERIFY(!r.error.isEmpty());
}

void TestSipUriNormalizer::atSignAtStart_invalid()
{
    const R r = SipUriNormalizer::normalize("@domain.com");
    QVERIFY(!r.isValid);
    QVERIFY(!r.error.isEmpty());
}

void TestSipUriNormalizer::atSignAtEnd_invalid()
{
    const R r = SipUriNormalizer::normalize("user@");
    QVERIFY(!r.isValid);
    QVERIFY(!r.error.isEmpty());
}

void TestSipUriNormalizer::containsSpace_invalid()
{
    const R r = SipUriNormalizer::normalize("sip: user@domain.com");
    QVERIFY(!r.isValid);
    QVERIFY(!r.error.isEmpty());
}

QTEST_MAIN(TestSipUriNormalizer)
#include "test_sip_uri_normalizer.moc"
