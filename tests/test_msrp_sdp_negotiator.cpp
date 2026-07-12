#include <QtTest/QtTest>

#include "msrp/MsrpSdpNegotiator.h"

class TestMsrpSdpNegotiator : public QObject
{
    Q_OBJECT

private slots:
    void parsesTcpOffer();
    void parsesTlsAnswer();
    void handlesMissingPath();
    void handlesDuplicatePathTolerantly();
    void handlesPortZeroRejection();
    void parsesAcceptTypesAndWrapped();
    void parsesDirectionAttributes();
    void handlesMultipleMessageSections();
    void doesNotAssumeAttributeOrder();
    void audioVideoSectionsIgnored();

    void roleActivePassiveValid();
    void rolePassiveActiveValid();
    void roleBothActiveInvalid();
    void roleBothPassiveInvalid();
    void roleHoldConnValid();
    void roleActPassUnresolvedInvalid();

    void buildsOfferBlock();
};

void TestMsrpSdpNegotiator::parsesTcpOffer()
{
    const QString sdp = QStringLiteral(
        "v=0\r\no=- 1 1 IN IP4 198.51.100.1\r\ns=-\r\nt=0 0\r\n"
        "m=message 2855 TCP/MSRP *\r\n"
        "a=path:msrp://198.51.100.1:2855/abc;tcp\r\n"
        "a=accept-types:text/plain message/cpim\r\n"
        "a=setup:actpass\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.size(), 1);
    QCOMPARE(blocks.first().transport, MsrpTransportProtocol::Tcp);
    QCOMPARE(blocks.first().port, 2855);
    QCOMPARE(blocks.first().setup, MsrpSetup::ActPass);
    QCOMPARE(blocks.first().path.size(), 1);
    QVERIFY(blocks.first().path.first().ok);
    QCOMPARE(blocks.first().acceptTypes, QStringList({QStringLiteral("text/plain"), QStringLiteral("message/cpim")}));
}

void TestMsrpSdpNegotiator::parsesTlsAnswer()
{
    const QString sdp = QStringLiteral(
        "m=message 2856 TCP/TLS/MSRP *\r\n"
        "a=path:msrps://relay.example.test:2856/def;tcp\r\n"
        "a=setup:passive\r\n"
        "a=accept-types:text/plain\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.size(), 1);
    QCOMPARE(blocks.first().transport, MsrpTransportProtocol::Tls);
    QCOMPARE(blocks.first().setup, MsrpSetup::Passive);
}

void TestMsrpSdpNegotiator::handlesMissingPath()
{
    const QString sdp = QStringLiteral("m=message 2855 TCP/MSRP *\r\na=setup:active\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.size(), 1);
    QCOMPARE(blocks.first().parseStatus, MsrpParseStatus::Partial);
    QVERIFY(!blocks.first().warnings.isEmpty());
}

void TestMsrpSdpNegotiator::handlesDuplicatePathTolerantly()
{
    const QString sdp = QStringLiteral(
        "m=message 2855 TCP/MSRP *\r\n"
        "a=path:msrp://h:2855/a;tcp msrp://h:2855/a;tcp\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.first().path.size(), 2);
}

void TestMsrpSdpNegotiator::handlesPortZeroRejection()
{
    const QString sdp = QStringLiteral("m=message 0 TCP/MSRP *\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.size(), 1);
    QVERIFY(blocks.first().rejected);
}

void TestMsrpSdpNegotiator::parsesAcceptTypesAndWrapped()
{
    const QString sdp = QStringLiteral(
        "m=message 2855 TCP/MSRP *\r\n"
        "a=path:msrp://h:2855/a;tcp\r\n"
        "a=accept-types:message/cpim\r\n"
        "a=accept-wrapped-types:text/plain message/imdn+xml application/im-iscomposing+xml\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.first().acceptWrappedTypes.size(), 3);
}

void TestMsrpSdpNegotiator::parsesDirectionAttributes()
{
    const QString sdp = QStringLiteral(
        "m=message 2855 TCP/MSRP *\r\na=path:msrp://h:2855/a;tcp\r\na=sendonly\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.first().direction, MsrpDirection::SendOnly);
}

void TestMsrpSdpNegotiator::handlesMultipleMessageSections()
{
    const QString sdp = QStringLiteral(
        "m=message 2855 TCP/MSRP *\r\na=path:msrp://h:2855/a;tcp\r\n"
        "m=message 2856 TCP/TLS/MSRP *\r\na=path:msrps://h:2856/b;tcp\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.size(), 2);
    QCOMPARE(blocks.at(0).transport, MsrpTransportProtocol::Tcp);
    QCOMPARE(blocks.at(1).transport, MsrpTransportProtocol::Tls);
}

void TestMsrpSdpNegotiator::doesNotAssumeAttributeOrder()
{
    const QString sdp = QStringLiteral(
        "m=message 2855 TCP/MSRP *\r\n"
        "a=accept-types:text/plain\r\n"
        "a=setup:active\r\n"
        "a=path:msrp://h:2855/a;tcp\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.first().setup, MsrpSetup::Active);
    QCOMPARE(blocks.first().path.size(), 1);
    QCOMPARE(blocks.first().acceptTypes.size(), 1);
}

void TestMsrpSdpNegotiator::audioVideoSectionsIgnored()
{
    const QString sdp = QStringLiteral(
        "m=audio 49170 RTP/AVP 0\r\na=sendrecv\r\n"
        "m=message 2855 TCP/MSRP *\r\na=path:msrp://h:2855/a;tcp\r\n"
        "m=video 51372 RTP/AVP 96\r\n");
    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdp);
    QCOMPARE(blocks.size(), 1);
}

void TestMsrpSdpNegotiator::roleActivePassiveValid()
{
    const auto r = MsrpSdpNegotiator::negotiateRole(MsrpSetup::Active, MsrpSetup::Passive);
    QVERIFY(r.valid);
    QCOMPARE(r.role, MsrpRole::ActiveConnector);
}

void TestMsrpSdpNegotiator::rolePassiveActiveValid()
{
    const auto r = MsrpSdpNegotiator::negotiateRole(MsrpSetup::Passive, MsrpSetup::Active);
    QVERIFY(r.valid);
    QCOMPARE(r.role, MsrpRole::PassiveListener);
}

void TestMsrpSdpNegotiator::roleBothActiveInvalid()
{
    const auto r = MsrpSdpNegotiator::negotiateRole(MsrpSetup::Active, MsrpSetup::Active);
    QVERIFY(!r.valid);
    QVERIFY(!r.errorMessage.isEmpty());
}

void TestMsrpSdpNegotiator::roleBothPassiveInvalid()
{
    const auto r = MsrpSdpNegotiator::negotiateRole(MsrpSetup::Passive, MsrpSetup::Passive);
    QVERIFY(!r.valid);
}

void TestMsrpSdpNegotiator::roleHoldConnValid()
{
    const auto r = MsrpSdpNegotiator::negotiateRole(MsrpSetup::HoldConn, MsrpSetup::Active);
    QVERIFY(r.valid);
    QCOMPARE(r.role, MsrpRole::HoldConn);
}

void TestMsrpSdpNegotiator::roleActPassUnresolvedInvalid()
{
    const auto r = MsrpSdpNegotiator::negotiateRole(MsrpSetup::ActPass, MsrpSetup::Active);
    QVERIFY(!r.valid);
}

void TestMsrpSdpNegotiator::buildsOfferBlock()
{
    const auto uri = MsrpPath::buildUri(false, QStringLiteral("198.51.100.1"), 2855, QStringLiteral("sess"));
    const QString block = MsrpSdpNegotiator::buildOfferBlock(
        uri, MsrpSetup::ActPass, {QStringLiteral("text/plain")}, {});
    QVERIFY(block.contains(QStringLiteral("m=message 2855 TCP/MSRP *")));
    QVERIFY(block.contains(QStringLiteral("a=setup:actpass")));
    QVERIFY(block.contains(QStringLiteral("a=path:msrp://198.51.100.1:2855/sess;tcp")));
}

QTEST_GUILESS_MAIN(TestMsrpSdpNegotiator)
#include "test_msrp_sdp_negotiator.moc"
