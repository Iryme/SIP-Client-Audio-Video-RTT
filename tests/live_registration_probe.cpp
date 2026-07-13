#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include <functional>
#include <iostream>

#include "core/Logger.h"
#include "security/CredentialStore.h"
#include "sip/RegistrationRefreshConfig.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

static QString envValue(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

static bool waitFor(int timeoutMs, const std::function<bool()> &predicate)
{
    if (predicate())
        return true;

    QEventLoop loop;
    QTimer timeout;
    QTimer poll;
    bool ok = false;

    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        loop.quit();
    });
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (predicate()) {
            ok = true;
            loop.quit();
        }
    });

    poll.start(50);
    timeout.start(timeoutMs);
    loop.exec();
    return ok || predicate();
}

static SipProfile liveProfile(const QString &server,
                              const QString &port,
                              const QString &domain,
                              const QString &username,
                              SipTransport transport,
                              const QString &outboundProxy)
{
    SipProfile profile;
    profile.profileId = QStringLiteral("task22c-live-kamailio-alice");
    profile.displayName = QStringLiteral("Task 22C Kamailio alice");
    profile.sipUsername = username;
    profile.sipDomain = domain;
    profile.sipUri = QStringLiteral("sip:%1@%2").arg(username, domain);
    // Registrar Request-URI must stay the served domain (e.g. sip2sip.info);
    // an outbound proxy is a routing-only hop added via config.sipConfig.proxies
    // (see SipAccount.cpp) — it must never replace the Request-URI host, or a
    // proxy that isn't itself the registrar for that domain will silently
    // ignore/drop the REGISTER instead of forwarding it.
    profile.registrar = port.isEmpty() || port == QStringLiteral("5060")
        ? server
        : QStringLiteral("%1:%2").arg(server, port);
    profile.authUsername = username;
    profile.transport = transport;
    profile.outboundProxy = outboundProxy;
    profile.enableRtt = false;
    profile.enableLmpe = false;
    profile.enableEtsiCompatibility = false;
    profile.createdAt = QDateTime::currentDateTimeUtc();
    profile.updatedAt = profile.createdAt;
    return profile;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString server = envValue("SIP_LIVE_SERVER");
    const QString portEnv = envValue("SIP_LIVE_PORT");
    const QString port = portEnv.isEmpty()
        ? QStringLiteral("5060")
        : portEnv;
    const QString domain = envValue("SIP_LIVE_DOMAIN");
    const QString username = envValue("SIP_LIVE_USERNAME");
    const QString password = envValue("SIP_LIVE_PASSWORD");
    const QString transportEnv = envValue("SIP_LIVE_TRANSPORT").toUpper();
    const SipTransport transport = transportEnv == QStringLiteral("TCP")
        ? SipTransport::TCP
        : SipTransport::UDP;
    const QString outboundProxy = envValue("SIP_LIVE_OUTBOUND_PROXY");

    if (server.isEmpty() || domain.isEmpty() || username.isEmpty() || password.isEmpty()) {
        std::cerr << "Missing SIP_LIVE_SERVER/SIP_LIVE_PORT/SIP_LIVE_DOMAIN/"
                     "SIP_LIVE_USERNAME/SIP_LIVE_PASSWORD environment.\n";
        return 2;
    }

    QObject::connect(&Logger::instance(), &Logger::entryAdded,
                     [](const LogEntry &entry) {
        if (entry.category == LogCategory::Sip || entry.category == LogCategory::Platform) {
            std::cout << qPrintable(QStringLiteral("[%1] [%2] %3")
                .arg(Logger::levelName(entry.level),
                     Logger::categoryName(entry.category),
                     entry.message))
                      << '\n';
        }
    });

    std::cout << "Task 22C live registration probe\n";
    std::cout << "Server: " << qPrintable(server) << ':' << qPrintable(port) << '\n';
    std::cout << "Domain/Realm: " << qPrintable(domain) << '\n';
    std::cout << "Username: " << qPrintable(username) << '\n';
    std::cout << "Transport: " << (transport == SipTransport::TCP ? "TCP" : "UDP") << '\n';

    SipManager &sip = SipManager::instance();
    RegistrationRetryPolicy retry;
    retry.maxAttempts = 0;
    sip.setRetryPolicy(retry);

    RegistrationRefreshConfig refresh;
    refresh.overrideDelayMs = 1500;
    sip.setRefreshConfig(refresh);

    if (!sip.initialize()) {
        std::cerr << "Initialize failed: " << qPrintable(sip.lastError()) << '\n';
        return 3;
    }
    std::cout << "Backend: " << qPrintable(sip.backendName()) << '\n';
    if (!sip.isPjsipAvailable()) {
        std::cerr << "Real PJSIP backend was not selected.\n";
        return 4;
    }

    SipProfileManager &profiles = SipProfileManager::instance();
    const SipProfile profile = liveProfile(server, port, domain, username, transport, outboundProxy);
    if (profiles.hasProfile(profile.profileId)) {
        if (!profiles.update(profile)) {
            std::cerr << "Profile update failed.\n";
            return 5;
        }
    } else if (profiles.add(profile).isEmpty()) {
        std::cerr << "Profile add failed.\n";
        return 5;
    }

    if (!profiles.setProfilePassword(profile.profileId, password)) {
        std::cerr << "Credential store failed.\n";
        return 6;
    }
    profiles.setActiveProfileId(profile.profileId);

    int refreshScheduledCount = 0;
    int refreshStartedCount = 0;
    QObject::connect(&sip, &SipManager::refreshScheduled,
                     [&refreshScheduledCount](int) { ++refreshScheduledCount; });
    QObject::connect(&sip, &SipManager::refreshStarted,
                     [&refreshStartedCount]() { ++refreshStartedCount; });

    std::cout << "REGISTER start\n";
    if (!sip.registerActiveProfile()) {
        std::cerr << "registerActiveProfile() returned false.\n";
        return 7;
    }

    if (!waitFor(20000, [&] { return sip.registrationState() == RegistrationState::Registered; })) {
        std::cerr << "REGISTER did not reach Registered. State="
                  << qPrintable(registrationStateName(sip.registrationState()))
                  << " status=" << sip.registrationStatusCode()
                  << " text=" << qPrintable(sip.registrationStatusText()) << '\n';
        return 8;
    }
    std::cout << "REGISTER final state: Registered status="
              << sip.registrationStatusCode() << '\n';

    if (!waitFor(10000, [&] { return refreshStartedCount > 0; })) {
        std::cerr << "Refresh did not start.\n";
        return 9;
    }
    if (!waitFor(10000, [&] { return refreshScheduledCount >= 2; })) {
        std::cerr << "Refresh did not complete/reschedule. State="
                  << qPrintable(registrationStateName(sip.registrationState()))
                  << '\n';
        return 10;
    }
    std::cout << "Refresh/re-REGISTER completed while Registered\n";

    std::cout << "UNREGISTER start\n";
    if (!sip.unregisterActiveProfile()) {
        std::cerr << "unregisterActiveProfile() returned false.\n";
        return 11;
    }
    if (!waitFor(20000, [&] { return sip.registrationState() == RegistrationState::Unregistered; })) {
        std::cerr << "UNREGISTER did not reach Unregistered. State="
                  << qPrintable(registrationStateName(sip.registrationState()))
                  << " status=" << sip.registrationStatusCode()
                  << " text=" << qPrintable(sip.registrationStatusText()) << '\n';
        return 12;
    }
    std::cout << "UNREGISTER final state: Unregistered status="
              << sip.registrationStatusCode() << '\n';

    sip.shutdown();
    return 0;
}
