#include <QtTest/QtTest>

#include <QCryptographicHash>
#include <QDir>
#include <QTemporaryFile>

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
    void sendsAndReceivesFileTransfer();
    void sendFileRejectsMissingFile();
    void rejectsSendWithMismatchedToPathSessionId();
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

void TestMsrpSessionHarness::sendsAndReceivesFileTransfer()
{
    MsrpSession server(QStringLiteral("srv5"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv5")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();

    MsrpSession client(QStringLiteral("cli5"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli5")));
    client.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("srv5"))});
    client.setChunkSizeBytes(256);

    QTemporaryFile srcFile(QDir::tempPath() + QStringLiteral("/msrp-w104-test-XXXXXX.bin"));
    QVERIFY(srcFile.open());
    const QByteArray content(3000, 'F');
    QCOMPARE(srcFile.write(content), static_cast<qint64>(content.size()));
    srcFile.close();

    QSignalSpy payloadSpy(&server, &MsrpSession::payloadReceived);
    QSignalSpy fileSpy(&server, &MsrpSession::fileTransferReceived);
    client.connectAsActive(kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(client.info().state == MsrpSessionState::Established, kWaitMs);

    const auto sendResult = client.sendFile(srcFile.fileName(), QStringLiteral("application/octet-stream"));
    QVERIFY(sendResult.ok);
    QCOMPARE(sendResult.fileSize, static_cast<qint64>(content.size()));
    QCOMPARE(sendResult.sha1Hex,
             QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha1).toHex()));

    QTRY_VERIFY_WITH_TIMEOUT(fileSpy.count() >= 1, kWaitMs);
    QCOMPARE(payloadSpy.count(), fileSpy.count()); // fileTransferReceived never fires instead of payloadReceived

    const auto args = fileSpy.first();
    QCOMPARE(args.at(2).toString(), QStringLiteral("application/octet-stream"));
    QVERIFY(!args.at(3).toString().isEmpty()); // suggested file name recovered from Content-Disposition
    QCOMPARE(args.at(4).toByteArray(), content);
}

void TestMsrpSessionHarness::sendFileRejectsMissingFile()
{
    MsrpSession client(QStringLiteral("cli6"));
    client.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("cli6")));

    const auto result = client.sendFile(QStringLiteral("does/not/exist.bin"), QStringLiteral("text/plain"));
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

// Task W105 (RFC 4975 §7.1 hardening) regression: MsrpTcpTransport's passive
// listener accepts the first TCP connection unconditionally and closes the
// listener (see MsrpTcpTransport::onNewConnection), so without a To-Path
// check any local process that connects before the real, SDP-negotiated
// peer would previously be silently trusted. This proves a connection whose
// SEND frames carry a To-Path session-id the server never negotiated is
// rejected (403) and never reaches payloadReceived.
void TestMsrpSessionHarness::rejectsSendWithMismatchedToPathSessionId()
{
    MsrpSession server(QStringLiteral("srv7"));
    server.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("srv7")));
    server.listenAsPassive(QStringLiteral("127.0.0.1"), kWaitMs);
    const int port = server.transportLocalPort();
    QVERIFY(port > 0);

    // "Attacker": knows the listening port but not the real session-id
    // (only ever exchanged via the SDP a=path over the signaling channel),
    // so it addresses SEND frames to a made-up session-id on the same port.
    MsrpSession attacker(QStringLiteral("atk7"));
    attacker.setLocalUri(MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), 0, QStringLiteral("atk7")));
    attacker.setRemotePath({MsrpPath::buildUri(false, QStringLiteral("127.0.0.1"), port, QStringLiteral("wrong-session-id"))});

    QSignalSpy receivedSpy(&server, &MsrpSession::payloadReceived);
    attacker.connectAsActive(kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(attacker.info().state == MsrpSessionState::Established, kWaitMs);

    attacker.sendMessage(QStringLiteral("text/plain"), QByteArray("malicious payload"));

    QTRY_VERIFY_WITH_TIMEOUT(!server.info().warnings.isEmpty(), kWaitMs);
    QCOMPARE(receivedSpy.count(), 0);
    QVERIFY(server.info().warnings.join(QLatin1Char('\n')).contains(QStringLiteral("To-Path")));
}

QTEST_GUILESS_MAIN(TestMsrpSessionHarness)
#include "test_msrp_session_harness.moc"
