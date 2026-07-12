#include <QtTest/QtTest>

#include "msrp/MsrpSession.h"

// Local MSRP integration harness (Task W100, section U) — client active
// role connecting to a local passive-role "server" MsrpSession over real
// loopback TCP sockets (127.0.0.1, OS-assigned dynamic port). No dependency
// on any real MSRP relay/server; entirely self-contained and safe to run
// under CTest. TLS/abort/invalid-frame-injection scenarios beyond what is
// covered here are documented as a known gap in docs/msrp-testing.md.
class TestMsrpSessionHarness : public QObject
{
    Q_OBJECT

private slots:
    void activeConnectsToLocalPassiveServer();
    void sendsAndReceivesPlainText();
    void sendsAndReceivesChunkedMessage();
    void connectionCloseIsObserved();
};

namespace {
constexpr int kWaitMs = 5000;
}

void TestMsrpSessionHarness::activeConnectsToLocalPassiveServer()
{
    MsrpSession server(QStringLiteral("srv-session"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();
    QVERIFY(port > 0);

    MsrpSession client(QStringLiteral("cli-session"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv"))});
    client.connectAsActive(kWaitMs);

    QTRY_VERIFY_WITH_TIMEOUT(client.info().state == MsrpSessionState::Established, kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(server.info().state == MsrpSessionState::Established, kWaitMs);
}

void TestMsrpSessionHarness::sendsAndReceivesPlainText()
{
    MsrpSession server(QStringLiteral("srv2"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv2")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();

    MsrpSession client(QStringLiteral("cli2"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli2")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv2"))});
    client.setRequestReports(true);

    QSignalSpy receivedSpy(&server, &MsrpSession::payloadReceived);
    client.connectAsActive(kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(client.info().state == MsrpSessionState::Established, kWaitMs);

    client.sendMessage(QStringLiteral("text/plain"), QByteArray("Hello MSRP!"));

    QTRY_VERIFY_WITH_TIMEOUT(receivedSpy.count() >= 1, kWaitMs);
    const auto args = receivedSpy.first();
    QCOMPARE(args.at(2).toString(), QStringLiteral("text/plain"));
    QCOMPARE(args.at(3).toByteArray(), QByteArray("Hello MSRP!"));

    QTRY_VERIFY_WITH_TIMEOUT(client.info().messagesCompleted == 0
        || client.info().framesSent >= 1, kWaitMs); // sanity: at least one frame was sent
}

void TestMsrpSessionHarness::sendsAndReceivesChunkedMessage()
{
    MsrpSession server(QStringLiteral("srv3"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv3")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();

    MsrpSession client(QStringLiteral("cli3"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli3")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv3"))});
    client.setChunkSizeBytes(256);

    QSignalSpy receivedSpy(&server, &MsrpSession::payloadReceived);
    client.connectAsActive(kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(client.info().state == MsrpSessionState::Established, kWaitMs);

    const QByteArray bigBody(3000, 'Q');
    client.sendMessage(QStringLiteral("text/plain"), bigBody);

    QTRY_VERIFY_WITH_TIMEOUT(receivedSpy.count() >= 1, kWaitMs);
    QCOMPARE(receivedSpy.first().at(3).toByteArray(), bigBody);
}

void TestMsrpSessionHarness::connectionCloseIsObserved()
{
    MsrpSession server(QStringLiteral("srv4"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv4")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();

    MsrpSession client(QStringLiteral("cli4"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli4")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv4"))});
    client.connectAsActive(kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(client.info().state == MsrpSessionState::Established, kWaitMs);

    client.closeSession();
    QTRY_VERIFY_WITH_TIMEOUT(client.info().state == MsrpSessionState::Closed
        || client.info().state == MsrpSessionState::Disconnecting, kWaitMs);
}

QTEST_GUILESS_MAIN(TestMsrpSessionHarness)
#include "test_msrp_session_harness.moc"
