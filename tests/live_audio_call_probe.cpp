#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include <functional>
#include <iostream>

#include "core/Logger.h"
#include "sip/CallStateMachine.h"
#include "sip/RegistrationRefreshConfig.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/SipAccount.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

static QString envValue(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

static QString normalizeTarget(const QString &target, const QString &domain)
{
    const QString trimmed = target.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive))
        return trimmed;
    if (trimmed.contains(QChar('@')))
        return QStringLiteral("sip:%1").arg(trimmed);
    if (!domain.trimmed().isEmpty())
        return QStringLiteral("sip:%1@%2").arg(trimmed, domain.trimmed());
    return QStringLiteral("sip:%1").arg(trimmed);
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
                              const QString &username)
{
    SipProfile profile;
    profile.profileId = QStringLiteral("task22c-live-audio-%1").arg(username);
    profile.displayName = QStringLiteral("Task 22C live audio");
    profile.sipUsername = username;
    profile.sipDomain = domain;
    profile.sipUri = QStringLiteral("sip:%1@%2").arg(username, domain);
    profile.registrar = port.isEmpty() || port == QStringLiteral("5060")
        ? server
        : QStringLiteral("%1:%2").arg(server, port);
    profile.authUsername = username;
    profile.transport = SipTransport::UDP;
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
    const QString port = portEnv.isEmpty() ? QStringLiteral("5060") : portEnv;
    const QString domain = envValue("SIP_LIVE_DOMAIN");
    const QString username = envValue("SIP_LIVE_USERNAME");
    const QString password = envValue("SIP_LIVE_PASSWORD");
    const QString targetRaw = envValue("SIP_LIVE_TARGET");

    if (server.isEmpty() || domain.isEmpty() || username.isEmpty()
        || password.isEmpty() || targetRaw.isEmpty()) {
        std::cerr << "Missing SIP_LIVE_SERVER/SIP_LIVE_PORT/SIP_LIVE_DOMAIN/"
                     "SIP_LIVE_USERNAME/SIP_LIVE_PASSWORD/SIP_LIVE_TARGET environment.\n";
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

    const QString target = normalizeTarget(targetRaw, domain);
    if (target.isEmpty()) {
        std::cerr << "SIP_LIVE_TARGET could not be normalized.\n";
        return 3;
    }

    std::cout << "Task 22C live audio call probe\n";
    std::cout << "Server: " << qPrintable(server) << ':' << qPrintable(port) << '\n';
    std::cout << "Domain/Realm: " << qPrintable(domain) << '\n';
    std::cout << "Username: " << qPrintable(username) << '\n';
    std::cout << "Target: " << qPrintable(target) << '\n';
    std::cout << "Transport: UDP\n";

    SipManager &sip = SipManager::instance();
    RegistrationRetryPolicy retry;
    retry.maxAttempts = 0;
    sip.setRetryPolicy(retry);

    RegistrationRefreshConfig refresh;
    refresh.overrideDelayMs = 1500;
    sip.setRefreshConfig(refresh);

    auto cleanup = [&]() {
        if (sip.callState() != CallState::Idle && sip.callState() != CallState::Failed) {
            sip.hangupCall();
            waitFor(15000, [&] {
                return sip.callState() == CallState::Idle
                    || sip.callState() == CallState::Failed;
            });
        }

        if (sip.registrationState() != RegistrationState::Unregistered) {
            sip.unregisterActiveProfile();
            waitFor(15000, [&] {
                return sip.registrationState() == RegistrationState::Unregistered;
            });
        }

        sip.shutdown();
    };

    if (!sip.initialize()) {
        std::cerr << "Initialize failed: " << qPrintable(sip.lastError()) << '\n';
        cleanup();
        return 4;
    }
    std::cout << "Backend: " << qPrintable(sip.backendName()) << '\n';
    if (!sip.isPjsipAvailable() || !sip.backendName().startsWith(QStringLiteral("PJSIP/"))) {
        std::cerr << "Real PJSIP backend was not selected.\n";
        cleanup();
        return 5;
    }

    SipProfileManager &profiles = SipProfileManager::instance();
    const SipProfile profile = liveProfile(server, port, domain, username);
    if (profiles.hasProfile(profile.profileId)) {
        if (!profiles.update(profile)) {
            std::cerr << "Profile update failed.\n";
            cleanup();
            return 6;
        }
    } else if (profiles.add(profile).isEmpty()) {
        std::cerr << "Profile add failed.\n";
        cleanup();
        return 6;
    }

    if (!profiles.setProfilePassword(profile.profileId, password)) {
        std::cerr << "Credential store failed.\n";
        cleanup();
        return 7;
    }
    profiles.setActiveProfileId(profile.profileId);

    bool audioMediaActive = false;

    QObject::connect(&sip, &SipManager::registrationStateChanged,
                     [](RegistrationState state, const QString &statusText, int statusCode) {
        std::cout << "REG state: " << qPrintable(registrationStateName(state))
                  << " status=" << statusCode
                  << " text=\"" << qPrintable(statusText) << "\"\n";
    });
    QObject::connect(&sip, &SipManager::callStateChanged,
                     [](CallState state, const QString &statusText, int statusCode) {
        std::cout << "CALL state: " << qPrintable(callStateName(state))
                  << " status=" << statusCode
                  << " text=\"" << qPrintable(statusText) << "\"\n";
    });
    QObject::connect(&sip, &SipManager::callConnected,
                     [](const QString &remoteUri) {
        std::cout << "CALL connected: " << qPrintable(remoteUri) << '\n';
    });
    QObject::connect(&sip, &SipManager::audioMediaConnected,
                     [&audioMediaActive] {
        audioMediaActive = true;
        std::cout << "AUDIO media connected\n";
    });
    QObject::connect(&sip, &SipManager::callDisconnected,
                     [](const QString &remoteUri, const QString &reason, int statusCode) {
        std::cout << "CALL disconnected: " << qPrintable(remoteUri)
                  << " status=" << statusCode
                  << " reason=\"" << qPrintable(reason) << "\"\n";
    });
    QObject::connect(&sip, &SipManager::callFailed,
                     [](const QString &remoteUri, const QString &reason, int statusCode) {
        std::cout << "CALL failed: " << qPrintable(remoteUri)
                  << " status=" << statusCode
                  << " reason=\"" << qPrintable(reason) << "\"\n";
    });

    std::cout << "REGISTER start\n";
    if (!sip.registerActiveProfile()) {
        std::cerr << "registerActiveProfile() returned false.\n";
        cleanup();
        return 8;
    }

    if (!waitFor(20000, [&] { return sip.registrationState() == RegistrationState::Registered; })) {
        std::cerr << "REGISTER did not reach Registered. State="
                  << qPrintable(registrationStateName(sip.registrationState()))
                  << " status=" << sip.registrationStatusCode()
                  << " text=" << qPrintable(sip.registrationStatusText()) << '\n';
        cleanup();
        return 9;
    }
    std::cout << "REGISTER final state: Registered status="
              << sip.registrationStatusCode() << '\n';

    std::cout << "CALL start\n";
    if (!sip.makeCall(target)) {
        std::cerr << "makeCall() returned false.\n";
        cleanup();
        return 10;
    }

    if (!waitFor(120000, [&] {
            const CallState state = sip.callState();
            return state == CallState::Active || state == CallState::Failed;
        })) {
        std::cerr << "Call did not reach Active within timeout. State="
                  << qPrintable(callStateName(sip.callState()))
                  << " status=" << qPrintable(sip.callStatusText()) << '\n';
        cleanup();
        return 11;
    }
    if (sip.callState() == CallState::Failed) {
        std::cerr << "Call failed before media became active. Status="
                  << qPrintable(sip.callStatusText()) << '\n';
        cleanup();
        return 12;
    }
    std::cout << "CALL reached Active\n";

    if (!waitFor(30000, [&] {
            return audioMediaActive || sip.callState() == CallState::Failed;
        })) {
        std::cerr << "Call left Active before audio media became detectable.\n";
        cleanup();
        return 13;
    }
    if (sip.callState() == CallState::Failed) {
        std::cerr << "Call failed before audio media became active.\n";
        cleanup();
        return 13;
    }
    if (!audioMediaActive) {
        std::cerr << "Audio media callback was not observed.\n";
        cleanup();
        return 13;
    }
    std::cout << "AUDIO media active\n";

    if (!waitFor(10000, [&] {
            return sip.callState() == CallState::Idle
                || sip.callState() == CallState::Failed;
        })) {
        std::cout << "Audio media stayed up long enough for manual validation\n";
    } else {
        std::cerr << "Call ended before the manual audio window completed.\n";
        cleanup();
        return 14;
    }

    std::cout << "HANGUP start\n";
    if (!sip.hangupCall()) {
        std::cerr << "hangupCall() returned false.\n";
        cleanup();
        return 15;
    }

    if (!waitFor(20000, [&] { return sip.callState() == CallState::Idle; })) {
        std::cerr << "Hangup did not reach Idle. State="
                  << qPrintable(callStateName(sip.callState()))
                  << " status=" << qPrintable(sip.callStatusText()) << '\n';
        cleanup();
        return 16;
    }
    std::cout << "HANGUP final state: Idle\n";

    std::cout << "UNREGISTER start\n";
    if (!sip.unregisterActiveProfile()) {
        std::cerr << "unregisterActiveProfile() returned false.\n";
        cleanup();
        return 17;
    }
    if (!waitFor(20000, [&] { return sip.registrationState() == RegistrationState::Unregistered; })) {
        std::cerr << "UNREGISTER did not reach Unregistered. State="
                  << qPrintable(registrationStateName(sip.registrationState()))
                  << " status=" << sip.registrationStatusCode()
                  << " text=" << qPrintable(sip.registrationStatusText()) << '\n';
        cleanup();
        return 18;
    }
    std::cout << "UNREGISTER final state: Unregistered status="
              << sip.registrationStatusCode() << '\n';

    cleanup();
    return 0;
}
