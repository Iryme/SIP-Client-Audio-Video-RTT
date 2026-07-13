#include <QtTest/QtTest>

#include "msrp/MsrpSession.h"
#include "msrp/MsrpTcpTransport.h"

// Task W107: MsrpSession::adoptExternalTransport() lets a session reuse an
// already-connected transport instead of opening its own via
// connectAsActive()/listenAsPassive() — the primitive relay-assisted MSRP
// needs to hand a session the same control connection MsrpRelayClient used
// for its AUTH handshake. Verified here against a plain TCP transport
// connected independently of any MsrpSession, proving the adoption path
// itself (signal wiring, immediate Established transition, real send/
// receive) without depending on MsrpRelayClient/a live relay.
class TestMsrpSessionRelayTransport : public QObject
{
    Q_OBJECT

private slots:
    void adoptedConnectedTransportReachesEstablishedImmediately();
    void adoptedTransportSendsAndReceives();
};

namespace { constexpr int kWaitMs = 5000; }

void TestMsrpSessionRelayTransport::adoptedConnectedTransportReachesEstablishedImmediately()
{
    MsrpSession server(QStringLiteral("relay-adopt-srv"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();
    QVERIFY(port > 0);

    auto externalTransport = std::make_unique<MsrpTcpTransport>();
    externalTransport->connectActive(QStringLiteral("127.0.0.1"), port, kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(externalTransport->isConnected(), kWaitMs);

    MsrpSession client(QStringLiteral("relay-adopt-cli"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv"))});
    client.adoptExternalTransport(std::move(externalTransport), MsrpRole::ActiveConnector);

    // Already connected at adoption time -> no waiting for a connected()
    // signal that will never fire again.
    QCOMPARE(client.info().state, MsrpSessionState::Established);
    QTRY_VERIFY_WITH_TIMEOUT(server.info().state == MsrpSessionState::Established, kWaitMs);
}

void TestMsrpSessionRelayTransport::adoptedTransportSendsAndReceives()
{
    MsrpSession server(QStringLiteral("relay-adopt-srv2"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv2")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();

    auto externalTransport = std::make_unique<MsrpTcpTransport>();
    externalTransport->connectActive(QStringLiteral("127.0.0.1"), port, kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(externalTransport->isConnected(), kWaitMs);

    MsrpSession client(QStringLiteral("relay-adopt-cli2"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli2")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv2"))});

    QSignalSpy receivedSpy(&server, &MsrpSession::payloadReceived);
    client.adoptExternalTransport(std::move(externalTransport), MsrpRole::ActiveConnector);
    QTRY_VERIFY_WITH_TIMEOUT(server.info().state == MsrpSessionState::Established, kWaitMs);

    client.sendMessage(QStringLiteral("text/plain"), QByteArray("via relay-adopted transport"));

    QTRY_VERIFY_WITH_TIMEOUT(receivedSpy.count() >= 1, kWaitMs);
    const auto args = receivedSpy.first();
    QCOMPARE(args.at(3).toByteArray(), QByteArray("via relay-adopted transport"));
}

QTEST_MAIN(TestMsrpSessionRelayTransport)
#include "test_msrp_session_relay_transport.moc"
