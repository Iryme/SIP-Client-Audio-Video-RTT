#include <QtTest/QtTest>

#include "sip/SdpMsrpDiagnosticsParser.h"

// Tests for SdpMsrpDiagnosticsParser — pure text parsing of SDP MSRP
// attributes (m=message / a=path / a=accept-types / a=setup / a=connection),
// no PJSIP/network needed and no MSRP session is ever started.

class TestSdpMsrpDiagnosticsParser : public QObject
{
    Q_OBJECT

private slots:
    void detectsTcpMsrpMediaBlock();
    void detectsTcpTlsMsrpAndSessionId();
    void sdpWithoutMessageMediaIsNotPresent();
    void audioOnlySdpIsNotPresent();
};

void TestSdpMsrpDiagnosticsParser::detectsTcpMsrpMediaBlock()
{
    const QString sdp =
        QStringLiteral("v=0\r\n"
                       "o=- 123 456 IN IP4 198.51.100.10\r\n"
                       "s=-\r\n"
                       "c=IN IP4 198.51.100.10\r\n"
                       "t=0 0\r\n"
                       "m=message 7654 TCP/MSRP *\r\n"
                       "a=accept-types:message/cpim text/plain\r\n"
                       "a=path:msrp://198.51.100.10:7654/session1;tcp\r\n"
                       "a=setup:active\r\n"
                       "a=connection:new\r\n");

    const SdpMsrpInfo info = SdpMsrpDiagnosticsParser::parse(sdp);

    QVERIFY(info.present);
    QCOMPARE(info.transportProtocol, QStringLiteral("TCP/MSRP"));
    QCOMPARE(info.acceptTypes, QStringLiteral("message/cpim text/plain"));
    QCOMPARE(info.setup, QStringLiteral("active"));
    QCOMPARE(info.connection, QStringLiteral("new"));
    QCOMPARE(info.sessionId, QStringLiteral("session1"));
}

void TestSdpMsrpDiagnosticsParser::detectsTcpTlsMsrpAndSessionId()
{
    const QString sdp =
        QStringLiteral("v=0\r\n"
                       "m=message 12763 TCP/TLS/MSRP *\r\n"
                       "a=path:msrps://relay.example.com:12763/9125d3736bb75f7a;tcp\r\n"
                       "a=accept-types:message/cpim\r\n"
                       "a=setup:passive\r\n");

    const SdpMsrpInfo info = SdpMsrpDiagnosticsParser::parse(sdp);

    QVERIFY(info.present);
    QCOMPARE(info.transportProtocol, QStringLiteral("TCP/TLS/MSRP"));
    QCOMPARE(info.sessionId, QStringLiteral("9125d3736bb75f7a"));
    QCOMPARE(info.setup, QStringLiteral("passive"));
}

void TestSdpMsrpDiagnosticsParser::sdpWithoutMessageMediaIsNotPresent()
{
    const QString sdp =
        QStringLiteral("v=0\r\n"
                       "m=audio 49170 RTP/AVP 0\r\n"
                       "a=rtpmap:0 PCMU/8000\r\n");

    const SdpMsrpInfo info = SdpMsrpDiagnosticsParser::parse(sdp);
    QVERIFY(!info.present);
}

void TestSdpMsrpDiagnosticsParser::audioOnlySdpIsNotPresent()
{
    const SdpMsrpInfo info = SdpMsrpDiagnosticsParser::parse(QString());
    QVERIFY(!info.present);
}

QTEST_GUILESS_MAIN(TestSdpMsrpDiagnosticsParser)
#include "test_sdp_msrp_diagnostics_parser.moc"
