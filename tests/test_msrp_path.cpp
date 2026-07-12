#include <QtTest/QtTest>
#include <QRegularExpression>

#include "msrp/MsrpPath.h"

class TestMsrpPath : public QObject
{
    Q_OBJECT

private slots:
    void parsesValidTcpUri();
    void parsesValidTlsUri();
    void parsesIpv4Host();
    void parsesIpv6Host();
    void parsesHostname();
    void rejectsInvalidScheme();
    void rejectsInvalidPort();
    void rejectsMissingSessionId();
    void parsesMultiEntryPath();
    void detectsDuplicatePathEntries();
    void generatesUrlSafeSessionId();
    void generatesUniqueSessionIds();
    void buildsRoundTrippableUri();
};

void TestMsrpPath::parsesValidTcpUri()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrp://alice.example.test:2855/abc123;tcp"));
    QVERIFY(uri.ok);
    QCOMPARE(uri.scheme, QStringLiteral("msrp"));
    QCOMPARE(uri.host, QStringLiteral("alice.example.test"));
    QCOMPARE(uri.port, 2855);
    QCOMPARE(uri.sessionId, QStringLiteral("abc123"));
    QCOMPARE(uri.transport, QStringLiteral("tcp"));
    QCOMPARE(uri.transportProtocol(), MsrpTransportProtocol::Tcp);
}

void TestMsrpPath::parsesValidTlsUri()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrps://bob.example.test:2856/xyz789;tcp"));
    QVERIFY(uri.ok);
    QCOMPARE(uri.transportProtocol(), MsrpTransportProtocol::Tls);
}

void TestMsrpPath::parsesIpv4Host()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrp://198.51.100.7:2855/sess1;tcp"));
    QVERIFY(uri.ok);
    QCOMPARE(uri.host, QStringLiteral("198.51.100.7"));
    QVERIFY(!uri.hostIsIpv6);
}

void TestMsrpPath::parsesIpv6Host()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrp://[2001:db8::1]:2855/sess2;tcp"));
    QVERIFY(uri.ok);
    QCOMPARE(uri.host, QStringLiteral("2001:db8::1"));
    QVERIFY(uri.hostIsIpv6);
    QCOMPARE(uri.port, 2855);
}

void TestMsrpPath::parsesHostname()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrp://relay.example.test/sess3;tcp"));
    QVERIFY(uri.ok);
    QCOMPARE(uri.host, QStringLiteral("relay.example.test"));
    QCOMPARE(uri.port, -1);
}

void TestMsrpPath::rejectsInvalidScheme()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("http://relay.example.test/sess;tcp"));
    QVERIFY(!uri.ok);
}

void TestMsrpPath::rejectsInvalidPort()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrp://relay.example.test:notaport/sess;tcp"));
    QVERIFY(!uri.ok);
}

void TestMsrpPath::rejectsMissingSessionId()
{
    const auto uri = MsrpPath::parseUri(QStringLiteral("msrp://relay.example.test:2855/;tcp"));
    QVERIFY(!uri.ok);
}

void TestMsrpPath::parsesMultiEntryPath()
{
    const auto uris = MsrpPath::parsePath(QStringLiteral(
        "msrp://relay1.example.test:2855/a1;tcp msrp://relay2.example.test:2855/a2;tcp"));
    QCOMPARE(uris.size(), 2);
    QVERIFY(uris.at(0).ok);
    QVERIFY(uris.at(1).ok);
    QCOMPARE(uris.at(0).sessionId, QStringLiteral("a1"));
    QCOMPARE(uris.at(1).sessionId, QStringLiteral("a2"));
}

void TestMsrpPath::detectsDuplicatePathEntries()
{
    const auto uris = MsrpPath::parsePath(QStringLiteral(
        "msrp://relay.example.test:2855/dup;tcp msrp://relay.example.test:2855/dup;tcp"));
    QCOMPARE(uris.size(), 2);
    QCOMPARE(uris.at(0).toString(), uris.at(1).toString());
}

void TestMsrpPath::generatesUrlSafeSessionId()
{
    const QString id = MsrpPath::generateSessionId();
    QVERIFY(id.size() >= 16);
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9\\-_]+$"));
    QVERIFY(re.match(id).hasMatch());
}

void TestMsrpPath::generatesUniqueSessionIds()
{
    QVERIFY(MsrpPath::generateSessionId() != MsrpPath::generateSessionId());
}

void TestMsrpPath::buildsRoundTrippableUri()
{
    const auto built = MsrpPath::buildUri(false, QStringLiteral("192.0.2.10"), 12345,
                                          QStringLiteral("session-abc"));
    QVERIFY(built.ok);
    const auto reparsed = MsrpPath::parseUri(built.toString());
    QVERIFY(reparsed.ok);
    QCOMPARE(reparsed.host, built.host);
    QCOMPARE(reparsed.port, built.port);
    QCOMPARE(reparsed.sessionId, built.sessionId);
}

QTEST_GUILESS_MAIN(TestMsrpPath)
#include "test_msrp_path.moc"
