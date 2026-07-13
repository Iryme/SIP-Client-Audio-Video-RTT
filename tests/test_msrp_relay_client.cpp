#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include "msrp/MsrpDigestAuth.h"
#include "msrp/MsrpFrameParser.h"
#include "msrp/MsrpFrameSerializer.h"
#include "msrp/MsrpRelayClient.h"

namespace {
constexpr int kWaitMs = 5000;

// A minimal, scripted RFC 4976 relay double: real TCP, real MSRP AUTH
// frames (parsed/serialized with the app's own MsrpFrameParser/Serializer,
// not hand-built bytes), but no actual message forwarding — enough to
// drive MsrpRelayClient's real state machine end-to-end without depending
// on any live third-party relay. Credentials used here
// (relay-test-user/relay-test-secret) exist only inside this test process.
class FakeMsrpRelay : public QObject
{
    Q_OBJECT
public:
    explicit FakeMsrpRelay(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeMsrpRelay::onNewConnection);
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return m_server.serverPort(); }

    QString realm{QStringLiteral("relay-test-realm")};
    QString nonce{QStringLiteral("relay-test-nonce-1")};
    QString username{QStringLiteral("relay-test-user")};
    QString password{QStringLiteral("relay-test-secret")};
    int expirySeconds{600};
    bool rejectAuthenticatedRequest{false};

signals:
    void authenticatedRequestReceived();

private slots:
    void onNewConnection()
    {
        QTcpSocket *socket = m_server.nextPendingConnection();
        m_socket = socket;
        connect(socket, &QTcpSocket::readyRead, this, &FakeMsrpRelay::onReadyRead);
    }

    void onReadyRead()
    {
        const QByteArray data = m_socket->readAll();
        const auto results = m_parser.feed(data);
        for (const auto &res : results) {
            if (res.status != MsrpFrameParser::Status::Complete)
                continue;
            handleAuthRequest(res.frame);
        }
    }

private:
    void handleAuthRequest(const MsrpFrame &request)
    {
        const QString authorization = request.unknownHeaders.value(QStringLiteral("Authorization"));
        MsrpFrame response;
        response.isRequest = false;
        response.transactionId = request.transactionId;
        response.toPath = request.fromPath;
        response.fromPath = request.toPath;

        if (authorization.isEmpty()) {
            response.responseCode = 401;
            response.responseComment = QStringLiteral("Unauthorized");
            response.unknownHeaders.insert(QStringLiteral("WWW-Authenticate"),
                QStringLiteral("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5, qop=\"auth\"")
                    .arg(realm, nonce));
        } else if (rejectAuthenticatedRequest) {
            response.responseCode = 401;
            response.responseComment = QStringLiteral("Unauthorized");
            response.unknownHeaders.insert(QStringLiteral("WWW-Authenticate"),
                QStringLiteral("Digest realm=\"%1\", nonce=\"%2\", algorithm=MD5, qop=\"auth\", stale=true")
                    .arg(realm, nonce));
        } else {
            emit authenticatedRequestReceived();
            response.responseCode = 200;
            response.responseComment = QStringLiteral("OK");
            response.unknownHeaders.insert(QStringLiteral("Use-Path"),
                QStringLiteral("msrp://127.0.0.1:%1/relay-session-1;tcp").arg(port()));
            response.unknownHeaders.insert(QStringLiteral("Expires"), QString::number(expirySeconds));
        }

        bool ok = false;
        const QByteArray bytes = MsrpFrameSerializer::serialize(response, &ok);
        if (ok && m_socket)
            m_socket->write(bytes);
    }

    QTcpServer m_server;
    QTcpSocket *m_socket{nullptr};
    MsrpFrameParser m_parser{65536};
};
} // namespace

class TestMsrpRelayClient : public QObject
{
    Q_OBJECT

private slots:
    void successfulAllocationAfterChallengeResponse();
    void wrongPasswordFailsAllocation();
    void allocationRefreshesBeforeExpiry();
    void unusableConfigFailsImmediatelyWithoutConnecting();

private:
    MsrpRelayConfig configFor(const FakeMsrpRelay &relay) const
    {
        MsrpRelayConfig config;
        config.mode = MsrpRelayMode::Required;
        config.relayHost = QStringLiteral("127.0.0.1");
        config.relayPort = relay.port();
        config.useTls = false;
        config.username = relay.username;
        config.connectTimeoutMs = kWaitMs;
        config.authTimeoutMs = kWaitMs;
        config.refreshMarginSeconds = 1;
        config.maxRetries = 1;
        return config;
    }
};

void TestMsrpRelayClient::successfulAllocationAfterChallengeResponse()
{
    FakeMsrpRelay relay;
    MsrpRelayClient client;
    client.configure(configFor(relay), relay.password);

    QSignalSpy readySpy(&client, &MsrpRelayClient::allocationReady);
    QSignalSpy failedSpy(&client, &MsrpRelayClient::allocationFailed);

    client.start();

    QTRY_VERIFY_WITH_TIMEOUT(readySpy.count() >= 1 || failedSpy.count() >= 1, kWaitMs);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(readySpy.count(), 1);

    QVERIFY(client.isAllocated());
    const MsrpRelayAllocation allocation = client.allocation();
    QVERIFY(allocation.valid);
    QVERIFY(allocation.allocatedUri().ok);
    QCOMPARE(allocation.allocatedUri().sessionId, QStringLiteral("relay-session-1"));
    QVERIFY(allocation.expiresAt > QDateTime::currentDateTimeUtc());
}

void TestMsrpRelayClient::wrongPasswordFailsAllocation()
{
    FakeMsrpRelay relay;
    MsrpRelayConfig config = configFor(relay);
    MsrpRelayClient client;
    // Fake relay never actually validates the digest in this harness's
    // "accept any authenticated retry" branch, so simulate a real rejection
    // by telling the relay to always challenge again (stale) — proving
    // MsrpRelayClient gives up after maxRetries rather than looping forever.
    relay.rejectAuthenticatedRequest = true;
    client.configure(config, QStringLiteral("wrong-password"));

    QSignalSpy failedSpy(&client, &MsrpRelayClient::allocationFailed);
    client.start();

    QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() >= 1, kWaitMs);
    QVERIFY(!client.isAllocated());
    QVERIFY(!client.allocation().valid);
}

void TestMsrpRelayClient::allocationRefreshesBeforeExpiry()
{
    FakeMsrpRelay relay;
    relay.expirySeconds = 2; // short expiry + refreshMarginSeconds=1 forces a fast refresh cycle
    MsrpRelayClient client;
    client.configure(configFor(relay), relay.password);

    QSignalSpy readySpy(&client, &MsrpRelayClient::allocationReady);
    QSignalSpy refreshedSpy(&client, &MsrpRelayClient::allocationRefreshed);

    client.start();
    QTRY_VERIFY_WITH_TIMEOUT(readySpy.count() >= 1, kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(refreshedSpy.count() >= 1, kWaitMs);
    QVERIFY(client.isAllocated());
}

void TestMsrpRelayClient::unusableConfigFailsImmediatelyWithoutConnecting()
{
    MsrpRelayClient client;
    MsrpRelayConfig config; // mode stays Disabled by default
    client.configure(config, QString());

    QSignalSpy failedSpy(&client, &MsrpRelayClient::allocationFailed);
    client.start();
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(client.state(), MsrpRelayClient::State::Failed);
}

QTEST_MAIN(TestMsrpRelayClient)
#include "test_msrp_relay_client.moc"
