#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include <iostream>

#include "msrp/MsrpRelayClient.h"
#include "msrp/MsrpRelayDiagnosticsStore.h"

// Manual live validation (Task W107): drives MsrpRelayClient's real RFC
// 4976 AUTH challenge/response + allocation against a real relay. Reads
// MSRP_RELAY_LIVE_HOST/PORT/USERNAME/PASSWORD (+ optional
// MSRP_RELAY_LIVE_TLS=0/1, default 1) from the environment — never a
// literal credential in this file. Intended for a disposable test account
// only; see docs/msrp-relay-sip2sip-validation.md for how this was actually
// run and what it found.
static QString envValue(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString host = envValue("MSRP_RELAY_LIVE_HOST");
    const QString portText = envValue("MSRP_RELAY_LIVE_PORT");
    const QString username = envValue("MSRP_RELAY_LIVE_USERNAME");
    const QString password = envValue("MSRP_RELAY_LIVE_PASSWORD");
    const QString tlsText = envValue("MSRP_RELAY_LIVE_TLS");

    if (host.isEmpty() || username.isEmpty() || password.isEmpty()) {
        std::cerr << "Missing MSRP_RELAY_LIVE_HOST/MSRP_RELAY_LIVE_USERNAME/MSRP_RELAY_LIVE_PASSWORD environment.\n";
        return 2;
    }

    MsrpRelayConfig config;
    config.mode = MsrpRelayMode::Required;
    config.relayHost = host;
    config.relayPort = portText.isEmpty() ? 2855 : portText.toInt();
    config.useTls = tlsText.isEmpty() ? true : (tlsText != QStringLiteral("0"));
    config.username = username;
    config.tlsVerifyPeer = true;
    config.connectTimeoutMs = 8000;
    config.authTimeoutMs = 8000;

    std::cout << "MSRP relay live probe (RFC 4976)\n";
    std::cout << "Relay: " << qPrintable(host) << ":" << config.relayPort
              << " tls=" << (config.useTls ? "yes" : "no") << '\n';
    std::cout << "Username: " << qPrintable(username) << '\n';

    MsrpRelayClient client;
    client.configure(config, password);

    bool done = false;
    bool ok = false;
    QString failureReason;
    QObject::connect(&client, &MsrpRelayClient::allocationReady,
                     [&](const MsrpRelayAllocation &allocation) {
        done = true;
        ok = true;
        std::cout << "Allocation SUCCESS\n";
        std::cout << "Allocated URI scheme: " << qPrintable(allocation.allocatedUri().scheme) << '\n';
        std::cout << "Allocated host: " << qPrintable(allocation.allocatedUri().host)
                  << ":" << allocation.allocatedUri().port << '\n';
        std::cout << "Use-Path entries: " << allocation.usePath.size() << '\n';
        std::cout << "Expires at (UTC): " << qPrintable(allocation.expiresAt.toString(Qt::ISODateWithMs)) << '\n';
    });
    QObject::connect(&client, &MsrpRelayClient::allocationFailed, [&](const QString &reason) {
        done = true;
        ok = false;
        failureReason = reason;
    });
    QObject::connect(&client, &MsrpRelayClient::stateChanged, [](MsrpRelayClient::State s) {
        std::cout << "State -> " << static_cast<int>(s) << '\n';
    });

    client.start();

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (done) loop.quit(); });
    poll.start(50);
    timeout.start(20000);
    loop.exec();

    std::cout << "\n--- relay diagnostics events (redacted) ---\n";
    for (const auto &ev : MsrpRelayDiagnosticsStore::instance().entries()) {
        std::cout << qPrintable(ev.timestamp.toString(Qt::ISODateWithMs)) << " kind=" << static_cast<int>(ev.kind)
                  << " responseCode=" << ev.responseCode
                  << " algorithm=" << qPrintable(ev.algorithm)
                  << " qopUsed=" << (ev.qopUsed ? "yes" : "no")
                  << " error=" << qPrintable(ev.error) << '\n';
    }

    if (!done) {
        std::cerr << "No AUTH response within timeout (relay unreachable or silently dropping AUTH).\n";
        return 3;
    }
    if (!ok) {
        std::cerr << "Allocation FAILED: " << qPrintable(failureReason) << '\n';
        return 4;
    }
    return 0;
}
