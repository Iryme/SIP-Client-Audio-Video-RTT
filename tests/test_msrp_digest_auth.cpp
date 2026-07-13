#include <QtTest/QtTest>

#include "msrp/MsrpDigestAuth.h"

// RFC 4976 MSRP relay AUTH digest primitives (Task W107). The core
// HA1/HA2/response math is exactly RFC 2617 HTTP Digest, so it is verified
// here against RFC 2617's own worked example (§3.5) — a deterministic test
// vector independent of any MSRP-specific usage, proving the hashing itself
// is correct before MsrpRelayClient's AUTH-method wiring is exercised.
class TestMsrpDigestAuth : public QObject
{
    Q_OBJECT

private slots:
    void rfc2617WorkedExampleMatchesKnownResponse();
    void challengeParsingHandlesQuotedCommasAndOptionalFields();
    void challengeParsingRejectsMissingRealmOrNonce();
    void buildAuthorizationHeaderOmitsQopFieldsWhenNoQopOffered();
    void buildAuthorizationHeaderIncludesQopFieldsWhenQopOffered();
    void cnonceIsNonEmptyAndVariesAcrossCalls();
};

void TestMsrpDigestAuth::rfc2617WorkedExampleMatchesKnownResponse()
{
    // RFC 2617 §3.5: username="Mufasa", realm="testrealm@host.com",
    // password="Circle Of Life", nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
    // nc=00000001, cnonce="0a4f113b", qop=auth, method=GET,
    // uri="/dir/index.html" -> HA1=939e7578ed9e3c518a452acee763bce9,
    // response=6629fae49393a05397450978507c4ef1.
    const QString ha1 = MsrpDigestAuth::computeHA1(
        QStringLiteral("Mufasa"), QStringLiteral("testrealm@host.com"), QStringLiteral("Circle Of Life"));
    QCOMPARE(ha1, QStringLiteral("939e7578ed9e3c518a452acee763bce9"));

    const QString ha2 = MsrpDigestAuth::computeHA2(QStringLiteral("GET"), QStringLiteral("/dir/index.html"));
    QCOMPARE(ha2, QStringLiteral("39aff3a2bab6126f332b942af96d3366"));

    const QString response = MsrpDigestAuth::computeResponse(
        ha1, QStringLiteral("dcd98b7102dd2f0e8b11d0f600bfb0c093"), QStringLiteral("00000001"),
        QStringLiteral("0a4f113b"), QStringLiteral("auth"), ha2);
    QCOMPARE(response, QStringLiteral("6629fae49393a05397450978507c4ef1"));
}

void TestMsrpDigestAuth::challengeParsingHandlesQuotedCommasAndOptionalFields()
{
    const QString header = QStringLiteral(
        "Digest realm=\"sip2sip.info, relay\", nonce=\"abc123\", algorithm=MD5, "
        "qop=\"auth\", opaque=\"xyz\", stale=true");
    const auto challenge = MsrpDigestAuth::parseChallenge(header);
    QVERIFY(challenge.ok);
    QCOMPARE(challenge.realm, QStringLiteral("sip2sip.info, relay"));
    QCOMPARE(challenge.nonce, QStringLiteral("abc123"));
    QCOMPARE(challenge.algorithm, QStringLiteral("MD5"));
    QCOMPARE(challenge.qop, QStringLiteral("auth"));
    QCOMPARE(challenge.opaque, QStringLiteral("xyz"));
    QVERIFY(challenge.stale);
}

void TestMsrpDigestAuth::challengeParsingRejectsMissingRealmOrNonce()
{
    QVERIFY(!MsrpDigestAuth::parseChallenge(QStringLiteral("Digest nonce=\"abc\"")).ok);
    QVERIFY(!MsrpDigestAuth::parseChallenge(QStringLiteral("Digest realm=\"r\"")).ok);
    QVERIFY(!MsrpDigestAuth::parseChallenge(QString()).ok);
}

void TestMsrpDigestAuth::buildAuthorizationHeaderOmitsQopFieldsWhenNoQopOffered()
{
    MsrpDigestAuth::Challenge challenge;
    challenge.ok = true;
    challenge.realm = QStringLiteral("relay.example.com");
    challenge.nonce = QStringLiteral("noncevalue");
    challenge.algorithm = QStringLiteral("MD5");
    // qop deliberately left empty.

    const QString header = MsrpDigestAuth::buildAuthorizationHeader(
        challenge, QStringLiteral("alice"), QStringLiteral("secret"),
        QStringLiteral("msrp://relay.example.com:2855"), QStringLiteral("cn"), QStringLiteral("00000001"));

    QVERIFY(header.startsWith(QStringLiteral("Digest ")));
    QVERIFY(header.contains(QStringLiteral("username=\"alice\"")));
    QVERIFY(header.contains(QStringLiteral("realm=\"relay.example.com\"")));
    QVERIFY(!header.contains(QStringLiteral("qop=")));
    QVERIFY(!header.contains(QStringLiteral("cnonce=")));
    QVERIFY(!header.contains(QStringLiteral("nc=")));
}

void TestMsrpDigestAuth::buildAuthorizationHeaderIncludesQopFieldsWhenQopOffered()
{
    MsrpDigestAuth::Challenge challenge;
    challenge.ok = true;
    challenge.realm = QStringLiteral("relay.example.com");
    challenge.nonce = QStringLiteral("noncevalue");
    challenge.algorithm = QStringLiteral("MD5");
    challenge.qop = QStringLiteral("auth");

    const QString header = MsrpDigestAuth::buildAuthorizationHeader(
        challenge, QStringLiteral("alice"), QStringLiteral("secret"),
        QStringLiteral("msrp://relay.example.com:2855"), QStringLiteral("cn"), QStringLiteral("00000001"));

    QVERIFY(header.contains(QStringLiteral("qop=auth")));
    QVERIFY(header.contains(QStringLiteral("cnonce=\"cn\"")));
    QVERIFY(header.contains(QStringLiteral("nc=00000001")));

    // The response digest itself must equal an independently-computed value
    // — this is the property MsrpRelayClient actually depends on.
    const QString ha1 = MsrpDigestAuth::computeHA1(QStringLiteral("alice"), challenge.realm, QStringLiteral("secret"));
    const QString ha2 = MsrpDigestAuth::computeHA2(QStringLiteral("AUTH"), QStringLiteral("msrp://relay.example.com:2855"));
    const QString expectedResponse = MsrpDigestAuth::computeResponse(
        ha1, challenge.nonce, QStringLiteral("00000001"), QStringLiteral("cn"), QStringLiteral("auth"), ha2);
    QVERIFY(header.contains(QStringLiteral("response=\"%1\"").arg(expectedResponse)));
}

void TestMsrpDigestAuth::cnonceIsNonEmptyAndVariesAcrossCalls()
{
    const QString a = MsrpDigestAuth::generateCnonce();
    const QString b = MsrpDigestAuth::generateCnonce();
    QVERIFY(!a.isEmpty());
    QVERIFY(!b.isEmpty());
    QVERIFY(a != b);
}

QTEST_MAIN(TestMsrpDigestAuth)
#include "test_msrp_digest_auth.moc"
