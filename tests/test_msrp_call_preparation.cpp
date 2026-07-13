#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include "msrp/MsrpCallPreparationController.h"
#include "msrp/MsrpFrameParser.h"
#include "msrp/MsrpFrameSerializer.h"
#include "msrp/MsrpRelayClient.h"

namespace {
constexpr int kWaitMs = 5000;

// Same minimal scripted RFC 4976 relay double used by test_msrp_relay_client
// (Task W107) — duplicated here rather than shared so this test file has no
// build dependency on another test's .cpp. Credentials exist only in this
// process.
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

class TestMsrpCallPreparation : public QObject
{
    Q_OBJECT

private slots:
    void noMsrpWantedResolvesToNoneMode();
    void relayDisabledResolvesToDirectMode();
    void relaySuccessProducesReadyOfferWithAllocation();
    void relayRequiredFailureEmitsFailed();
    void relayAutomaticFailureFallsBackToDirect();
    void cancelDuringAllocationSuppressesLateSignals();

private:
    MsrpRelayConfig configFor(const FakeMsrpRelay &relay, MsrpRelayMode mode) const
    {
        MsrpRelayConfig config;
        config.mode = mode;
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

void TestMsrpCallPreparation::noMsrpWantedResolvesToNoneMode()
{
    MsrpCallPreparationController controller;
    QSignalSpy readySpy(&controller, &MsrpCallPreparationController::ready);

    MsrpRelayConfig cfg; // Disabled by default
    controller.start(/*wantsMsrp=*/false, cfg, QString(), QStringLiteral("prep-1"), 5000);

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kWaitMs);
    const PreparedMsrpOffer offer = qvariant_cast<PreparedMsrpOffer>(readySpy.at(0).at(0));
    QVERIFY(offer.ready);
    QCOMPARE(offer.mode, MsrpCallTransportMode::None);
}

void TestMsrpCallPreparation::relayDisabledResolvesToDirectMode()
{
    MsrpCallPreparationController controller;
    QSignalSpy readySpy(&controller, &MsrpCallPreparationController::ready);

    MsrpRelayConfig cfg; // Disabled by default
    controller.start(/*wantsMsrp=*/true, cfg, QString(), QStringLiteral("prep-2"), 5000);

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, kWaitMs);
    const PreparedMsrpOffer offer = qvariant_cast<PreparedMsrpOffer>(readySpy.at(0).at(0));
    QVERIFY(offer.ready);
    QCOMPARE(offer.mode, MsrpCallTransportMode::Direct);
    QVERIFY(!offer.relayClient);
}

void TestMsrpCallPreparation::relaySuccessProducesReadyOfferWithAllocation()
{
    FakeMsrpRelay relay;
    MsrpCallPreparationController controller;
    QSignalSpy readySpy(&controller, &MsrpCallPreparationController::ready);
    QSignalSpy failedSpy(&controller, &MsrpCallPreparationController::failed);

    controller.start(true, configFor(relay, MsrpRelayMode::Required), relay.password,
                      QStringLiteral("prep-3"), kWaitMs);

    QTRY_VERIFY_WITH_TIMEOUT(readySpy.count() >= 1 || failedSpy.count() >= 1, kWaitMs);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(readySpy.count(), 1);

    const PreparedMsrpOffer offer = qvariant_cast<PreparedMsrpOffer>(readySpy.at(0).at(0));
    QVERIFY(offer.ready);
    QCOMPARE(offer.mode, MsrpCallTransportMode::Relay);
    QVERIFY(offer.advertisedUri.ok);
    QCOMPARE(offer.advertisedUri.sessionId, QStringLiteral("relay-session-1"));
    QVERIFY(offer.relayAllocation.valid);
    QVERIFY(offer.relayClient != nullptr);
    QVERIFY(offer.relayClient->isAllocated());
}

void TestMsrpCallPreparation::relayRequiredFailureEmitsFailed()
{
    FakeMsrpRelay relay;
    relay.rejectAuthenticatedRequest = true;
    MsrpCallPreparationController controller;
    QSignalSpy readySpy(&controller, &MsrpCallPreparationController::ready);
    QSignalSpy failedSpy(&controller, &MsrpCallPreparationController::failed);

    controller.start(true, configFor(relay, MsrpRelayMode::Required),
                      QStringLiteral("wrong-password"), QStringLiteral("prep-4"), kWaitMs);

    QTRY_VERIFY_WITH_TIMEOUT(failedSpy.count() >= 1, kWaitMs);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(controller.state(), MsrpCallPreparationController::State::Failed);
}

void TestMsrpCallPreparation::relayAutomaticFailureFallsBackToDirect()
{
    FakeMsrpRelay relay;
    relay.rejectAuthenticatedRequest = true;
    MsrpCallPreparationController controller;
    QSignalSpy readySpy(&controller, &MsrpCallPreparationController::ready);
    QSignalSpy failedSpy(&controller, &MsrpCallPreparationController::failed);

    controller.start(true, configFor(relay, MsrpRelayMode::Automatic),
                      QStringLiteral("wrong-password"), QStringLiteral("prep-5"), kWaitMs);

    QTRY_VERIFY_WITH_TIMEOUT(readySpy.count() >= 1, kWaitMs);
    QCOMPARE(failedSpy.count(), 0);
    const PreparedMsrpOffer offer = qvariant_cast<PreparedMsrpOffer>(readySpy.at(0).at(0));
    QCOMPARE(offer.mode, MsrpCallTransportMode::Direct);
}

void TestMsrpCallPreparation::cancelDuringAllocationSuppressesLateSignals()
{
    FakeMsrpRelay relay;
    MsrpCallPreparationController controller;
    QSignalSpy readySpy(&controller, &MsrpCallPreparationController::ready);
    QSignalSpy failedSpy(&controller, &MsrpCallPreparationController::failed);
    QSignalSpy cancelledSpy(&controller, &MsrpCallPreparationController::cancelled);

    controller.start(true, configFor(relay, MsrpRelayMode::Required), relay.password,
                      QStringLiteral("prep-6"), kWaitMs);
    controller.cancel();

    QCOMPARE(cancelledSpy.count(), 1);
    // Give the fake relay's real network round-trip a chance to complete in
    // the background; neither ready() nor failed() must ever fire for a
    // cancelled preparation, no matter how the allocation would have
    // resolved.
    QTest::qWait(500);
    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
}

QTEST_MAIN(TestMsrpCallPreparation)
#include "test_msrp_call_preparation.moc"
