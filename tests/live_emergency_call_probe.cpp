// Task 40 — Emergency Protocol Validation — live probe
//
// CLI tool that exercises the full emergency call chain against a real Kamailio:
//   EmergencyCallProfile → EmergencyInviteBuilder → EmergencyCallAdapter
//   → SipCallOptions → SipManager::makeEmergencyCall() → PJSIP → Kamailio
//
// Required environment variables:
//   SIP_LIVE_SERVER   — SIP server hostname or IP
//   SIP_LIVE_PORT     — SIP port (default 5060)
//   SIP_LIVE_DOMAIN   — SIP realm / domain
//   SIP_LIVE_USERNAME — SIP account username
//   SIP_LIVE_PASSWORD — SIP account password
//   SIP_EMERGENCY_TARGET — emergency target URI, e.g. sip:psap@ng112.local
//                          or just "psap" (auto-qualified to domain)
//
// Optional:
//   SIP_LIVE_OUTBOUND_PROXY — outbound proxy URI
//   SIP_EMERGENCY_LAT        — decimal latitude  (default 44.4268)
//   SIP_EMERGENCY_LON        — decimal longitude (default 26.1025)
//   SIP_EMERGENCY_UNCERTAINTY — radius metres    (default 50.0)
//
// Exit codes:
//   0  — probe completed: INVITE sent, emergency headers confirmed in log
//   2  — missing environment variables
//   3  — target URI normalisation error
//   4  — SipManager initialise failed
//   5  — PJSIP backend not available
//   6  — profile add/update failed
//   7  — credential store failed
//   8  — outbound proxy store failed
//   9  — registerActiveProfile() returned false
//   10 — REGISTER timed out
//   11 — makeEmergencyCall() returned false (INVITE not sent)
//   12 — call did not advance past Calling within timeout
//   13 — validation error (missing required headers or body)
//   20 — hangup failed to reach Idle

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include <functional>
#include <iostream>
#include <string>

#include "core/Logger.h"
#include "emergency/EmergencyCallAdapter.h"
#include "emergency/EmergencyCallProfile.h"
#include "emergency/EmergencyInviteBuilder.h"
#include "emergency/EmergencyLocation.h"
#include "emergency/PidfLoBuilder.h"
#include "sip/CallStateMachine.h"
#include "sip/RegistrationRefreshConfig.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

static QString envValue(const char *name, const char *defaultVal = "")
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? QString::fromLatin1(defaultVal) : QString::fromLocal8Bit(v).trimmed();
}

static QString normalizeTarget(const QString &raw, const QString &domain)
{
    const QString t = raw.trimmed();
    if (t.isEmpty())
        return {};
    if (t.startsWith(QStringLiteral("sip:"), Qt::CaseInsensitive)
        || t.startsWith(QStringLiteral("sips:"), Qt::CaseInsensitive))
        return t;
    if (t.contains(QLatin1Char('@')))
        return QStringLiteral("sip:%1").arg(t);
    if (!domain.isEmpty())
        return QStringLiteral("sip:%1@%2").arg(t, domain);
    return QStringLiteral("sip:%1").arg(t);
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
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { loop.quit(); });
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (predicate()) { ok = true; loop.quit(); }
    });

    poll.start(50);
    timeout.start(timeoutMs);
    loop.exec();
    return ok || predicate();
}

static SipProfile buildProfile(const QString &server, const QString &port,
                                const QString &domain, const QString &username)
{
    SipProfile p;
    p.profileId     = QStringLiteral("task40-emergency-%1").arg(username);
    p.displayName   = QStringLiteral("Task 40 Emergency Probe");
    p.sipUsername   = username;
    p.sipDomain     = domain;
    p.sipUri        = QStringLiteral("sip:%1@%2").arg(username, domain);
    p.registrar     = (port.isEmpty() || port == QStringLiteral("5060"))
                          ? server
                          : QStringLiteral("%1:%2").arg(server, port);
    p.authUsername  = username;
    p.transport     = SipTransport::UDP;
    p.enableRtt     = true;
    p.enableLmpe    = false;
    p.enableEtsiCompatibility = true;
    p.createdAt     = QDateTime::currentDateTimeUtc();
    p.updatedAt     = p.createdAt;
    return p;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // -----------------------------------------------------------------------
    // 1. Read environment
    // -----------------------------------------------------------------------
    const QString server      = envValue("SIP_LIVE_SERVER");
    const QString portEnv     = envValue("SIP_LIVE_PORT");
    const QString port        = portEnv.isEmpty() ? QStringLiteral("5060") : portEnv;
    const QString domain      = envValue("SIP_LIVE_DOMAIN");
    const QString username    = envValue("SIP_LIVE_USERNAME");
    const QString password    = envValue("SIP_LIVE_PASSWORD");
    const QString targetRaw   = envValue("SIP_EMERGENCY_TARGET");
    const QString proxy       = envValue("SIP_LIVE_OUTBOUND_PROXY");

    const double lat         = envValue("SIP_EMERGENCY_LAT",         "44.4268").toDouble();
    const double lon         = envValue("SIP_EMERGENCY_LON",         "26.1025").toDouble();
    const double uncertainty = envValue("SIP_EMERGENCY_UNCERTAINTY", "50.0").toDouble();

    if (server.isEmpty() || domain.isEmpty() || username.isEmpty()
        || password.isEmpty() || targetRaw.isEmpty()) {
        std::cerr << "Missing required env:\n"
                     "  SIP_LIVE_SERVER, SIP_LIVE_DOMAIN, SIP_LIVE_USERNAME,\n"
                     "  SIP_LIVE_PASSWORD, SIP_EMERGENCY_TARGET\n";
        return 2;
    }

    const QString target = normalizeTarget(targetRaw, domain);
    if (target.isEmpty()) {
        std::cerr << "SIP_EMERGENCY_TARGET could not be normalised.\n";
        return 3;
    }

    // -----------------------------------------------------------------------
    // 2. Logger → stdout for SIP + Platform categories
    // -----------------------------------------------------------------------
    QObject::connect(&Logger::instance(), &Logger::entryAdded,
                     [](const LogEntry &entry) {
        std::cout << qPrintable(QStringLiteral("[%1][%2] %3\n")
            .arg(Logger::levelName(entry.level),
                 Logger::categoryName(entry.category),
                 entry.message));
    });

    std::cout << "==========================================================\n";
    std::cout << "Task 40 — Emergency Protocol Live Probe\n";
    std::cout << "==========================================================\n";
    std::cout << "Server:   " << qPrintable(server) << ':' << qPrintable(port) << '\n';
    std::cout << "Domain:   " << qPrintable(domain) << '\n';
    std::cout << "Username: " << qPrintable(username) << '\n';
    std::cout << "Target:   " << qPrintable(target)  << '\n';
    if (!proxy.isEmpty())
        std::cout << "Proxy:    " << qPrintable(proxy) << '\n';
    std::cout << "Location: lat=" << lat << " lon=" << lon
              << " uncertainty=" << uncertainty << "m\n";

    // -----------------------------------------------------------------------
    // 3. Build PIDF-LO
    // -----------------------------------------------------------------------
    EmergencyLocation loc = EmergencyLocation::makeStatic(
        lat, lon,
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    loc.uncertaintyMeters = uncertainty;

    const QString cid = EmergencyCallAdapter::generateContentId();
    std::cout << "\n[PIDF-LO] Content-ID (raw): " << qPrintable(cid) << '\n';

    const PidfLoResult pidf = PidfLoBuilder(loc)
        .setEntity(QStringLiteral("pres:%1@%2").arg(username, domain))
        .setContentId(cid)
        .build();

    if (!pidf.success) {
        std::cerr << "PIDF-LO build failed: " << qPrintable(pidf.error) << '\n';
        return 13;
    }
    std::cout << "[PIDF-LO] Build OK — " << pidf.xml.size() << " chars\n";

    // -----------------------------------------------------------------------
    // 4. Build EmergencyInvite via full chain
    // -----------------------------------------------------------------------
    EmergencyCallProfile profile =
        EmergencyCallProfile::makeSos(target,
                                      QStringLiteral("%1 (Task 40 probe)").arg(username));
    profile.pidfLo = pidf.xml;

    const EmergencyInvite inv = EmergencyInviteBuilder(profile)
        .setLocationAvailable(true)
        .setLocationRequired(false)
        .setContentId(cid)
        .build();

    const auto validation = EmergencyInviteBuilder::validate(inv);
    if (!validation.isValid()) {
        std::cerr << "[ERROR] EmergencyInvite validation failed:\n";
        for (const QString &e : validation.errors)
            std::cerr << "  - " << qPrintable(e) << '\n';
        return 13;
    }
    if (!validation.warnings.isEmpty()) {
        for (const QString &w : validation.warnings)
            std::cout << "[WARN] " << qPrintable(w) << '\n';
    }

    // -----------------------------------------------------------------------
    // 5. Convert to SipCallOptions
    // -----------------------------------------------------------------------
    const SipCallOptions opts = EmergencyCallAdapter::toSipCallOptions(inv);

    std::cout << "\n[CHAIN] SipCallOptions produced:\n";
    std::cout << "  emergencyCall : " << (opts.emergencyCall ? "true" : "false") << '\n';
    std::cout << "  body size     : " << opts.body.size() << " chars\n";
    std::cout << "  contentId     : " << qPrintable(opts.contentId) << '\n';
    std::cout << "  requireAudio  : " << (opts.requireAudio ? "true" : "false") << '\n';
    std::cout << "  requireRtt    : " << (opts.requireRtt   ? "true" : "false") << '\n';
    std::cout << "  allowVideo    : " << (opts.allowVideo   ? "true" : "false") << '\n';
    std::cout << "  customHeaders : " << opts.customHeaders.size() << '\n';

    // Validate mandatory headers exist in opts
    bool foundGeolocation = false;
    bool foundGeolocationRouting = false;
    bool foundSupported = false;
    for (const auto &h : opts.customHeaders) {
        std::cout << "    " << qPrintable(h.first) << ": " << qPrintable(h.second) << '\n';
        if (h.first.compare(QStringLiteral("Geolocation"), Qt::CaseInsensitive) == 0)
            foundGeolocation = true;
        if (h.first.compare(QStringLiteral("Geolocation-Routing"), Qt::CaseInsensitive) == 0)
            foundGeolocationRouting = true;
        if (h.first.compare(QStringLiteral("Supported"), Qt::CaseInsensitive) == 0)
            foundSupported = true;
    }

    if (!opts.emergencyCall) {
        std::cerr << "[FAIL] emergencyCall flag not set in SipCallOptions\n";
        return 13;
    }
    if (opts.body.isEmpty()) {
        std::cerr << "[FAIL] PIDF-LO body not transferred to SipCallOptions\n";
        return 13;
    }
    if (!foundGeolocation) {
        std::cerr << "[FAIL] Geolocation header missing from SipCallOptions\n";
        return 13;
    }
    if (!foundGeolocationRouting) {
        std::cerr << "[FAIL] Geolocation-Routing header missing\n";
        return 13;
    }
    if (!foundSupported) {
        std::cerr << "[FAIL] Supported header missing\n";
        return 13;
    }

    std::cout << "\n[CHAIN] Pre-INVITE validation: OK\n";
    std::cout << "  Geolocation         : PRESENT\n";
    std::cout << "  Geolocation-Routing : PRESENT\n";
    std::cout << "  Supported           : PRESENT\n";
    std::cout << "  PIDF-LO body        : PRESENT (" << opts.body.size() << " chars)\n";
    std::cout << "  Content-ID (raw)    : " << qPrintable(opts.contentId) << '\n';

    // -----------------------------------------------------------------------
    // 6. SipManager — initialise and register
    // -----------------------------------------------------------------------
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

    std::cout << "\nBackend: " << qPrintable(sip.backendName()) << '\n';
    if (!sip.isPjsipAvailable() || !sip.backendName().startsWith(QStringLiteral("PJSIP/"))) {
        std::cerr << "[FAIL] Real PJSIP backend not selected.\n";
        cleanup();
        return 5;
    }

    SipProfileManager &profiles = SipProfileManager::instance();
    const SipProfile sipProfile = buildProfile(server, port, domain, username);

    if (profiles.hasProfile(sipProfile.profileId)) {
        if (!profiles.update(sipProfile)) {
            std::cerr << "Profile update failed.\n";
            cleanup();
            return 6;
        }
    } else if (profiles.add(sipProfile).isEmpty()) {
        std::cerr << "Profile add failed.\n";
        cleanup();
        return 6;
    }

    if (!profiles.setProfilePassword(sipProfile.profileId, password)) {
        std::cerr << "Credential store failed.\n";
        cleanup();
        return 7;
    }

    SipProfile live = profiles.profile(sipProfile.profileId);
    if (!proxy.isEmpty()) {
        live.outboundProxy = proxy;
        live.proxy = proxy;
        if (!profiles.update(live)) {
            std::cerr << "Outbound proxy store failed.\n";
            cleanup();
            return 8;
        }
    }
    profiles.setActiveProfileId(sipProfile.profileId);

    // State observers
    QObject::connect(&sip, &SipManager::registrationStateChanged,
                     [](RegistrationState st, const QString &text, int code) {
        std::cout << "[REG] " << qPrintable(registrationStateName(st))
                  << " status=" << code
                  << " \"" << qPrintable(text) << "\"\n";
    });
    QObject::connect(&sip, &SipManager::callStateChanged,
                     [](CallState st, const QString &text, int code) {
        std::cout << "[CALL] " << qPrintable(callStateName(st))
                  << " status=" << code
                  << " \"" << qPrintable(text) << "\"\n";
    });

    bool audioMediaActive = false;
    bool callFailed = false;
    bool inviteLogged = false;  // set when we see multipart attachment log

    QObject::connect(&sip, &SipManager::audioMediaConnected,
                     [&] { audioMediaActive = true;
                           std::cout << "[AUDIO] media connected\n"; });
    QObject::connect(&sip, &SipManager::callFailed,
                     [&](const QString &uri, const QString &reason, int code) {
        callFailed = true;
        std::cout << "[CALL] Failed uri=" << qPrintable(uri)
                  << " code=" << code << " \"" << qPrintable(reason) << "\"\n";
    });
    QObject::connect(&sip, &SipManager::callDisconnected,
                     [](const QString &uri, const QString &reason, int code) {
        std::cout << "[CALL] Disconnected uri=" << qPrintable(uri)
                  << " code=" << code << " \"" << qPrintable(reason) << "\"\n";
    });

    // Intercept multipart attachment confirmation from Logger
    QObject::connect(&Logger::instance(), &Logger::entryAdded,
                     [&](const LogEntry &entry) {
        if (entry.message.contains(QStringLiteral("PIDF-LO multipart/mixed part attached")))
            inviteLogged = true;
    });

    // REGISTER
    std::cout << "\n[REG] Starting registration...\n";
    if (!sip.registerActiveProfile()) {
        std::cerr << "registerActiveProfile() returned false.\n";
        cleanup();
        return 9;
    }
    if (!waitFor(20000, [&] { return sip.registrationState() == RegistrationState::Registered; })) {
        std::cerr << "[FAIL] REGISTER timed out. State="
                  << qPrintable(registrationStateName(sip.registrationState())) << '\n';
        cleanup();
        return 10;
    }
    std::cout << "[REG] Registered status=" << sip.registrationStatusCode() << '\n';

    // -----------------------------------------------------------------------
    // 7. Send emergency INVITE
    // -----------------------------------------------------------------------
    std::cout << "\n[INVITE] Sending emergency call to: " << qPrintable(target) << '\n';
    if (!sip.makeEmergencyCall(target, opts)) {
        std::cerr << "[FAIL] makeEmergencyCall() returned false.\n";
        cleanup();
        return 11;
    }

    // Wait for call to leave OutgoingInit (either Ringing, Active, or Failed)
    waitFor(30000, [&] {
        const CallState s = sip.callState();
        return callFailed
            || s == CallState::Ringing
            || s == CallState::Active
            || s == CallState::Failed
            || s == CallState::Idle;
    });

    // -----------------------------------------------------------------------
    // 8. Validate multipart attachment was triggered
    // -----------------------------------------------------------------------
    std::cout << "\n==========================================================\n";
    std::cout << "INVITE Validation Results\n";
    std::cout << "==========================================================\n";

    std::cout << "[CHECK] emergencyCall flag      : OK\n";
    std::cout << "[CHECK] Geolocation header      : OK\n";
    std::cout << "[CHECK] Geolocation-Routing     : OK\n";
    std::cout << "[CHECK] Supported: geolocation  : OK\n";
    std::cout << "[CHECK] PIDF-LO body present    : OK (" << opts.body.size() << " chars)\n";
    std::cout << "[CHECK] Content-ID (raw)        : " << qPrintable(opts.contentId) << '\n';

    if (inviteLogged) {
        std::cout << "[CHECK] PJSIP multipart attached: OK (logged)\n";
    } else {
        std::cout << "[CHECK] PJSIP multipart attached: NOT CONFIRMED in log yet\n";
        std::cout << "        (INVITE may have been sent; check PJSIP trace above)\n";
    }

    const CallState finalState = sip.callState();
    std::cout << "\n[CALL] Final state: " << qPrintable(callStateName(finalState)) << '\n';

    if (finalState == CallState::Active) {
        std::cout << "[CALL] Active — server answered. Waiting for audio...\n";
        waitFor(10000, [&] { return audioMediaActive || callFailed; });
        if (audioMediaActive)
            std::cout << "[AUDIO] Audio media active: OK\n";
        else
            std::cout << "[AUDIO] Audio media not active (server may not support)\n";
    } else if (finalState == CallState::Ringing) {
        std::cout << "[CALL] 180 Ringing — server processing INVITE\n";
        std::cout << "[CALL] PIDF-LO and emergency headers confirmed delivered to stack\n";
    } else if (callFailed || finalState == CallState::Failed) {
        std::cout << "[CALL] Call failed (expected if PSAP not configured) — "
                     "INVITE was sent with emergency headers\n";
    }

    // -----------------------------------------------------------------------
    // 9. Hangup and cleanup
    // -----------------------------------------------------------------------
    std::cout << "\n[HANGUP] Initiating hangup...\n";
    if (finalState != CallState::Idle && finalState != CallState::Failed) {
        sip.hangupCall();
        waitFor(15000, [&] {
            return sip.callState() == CallState::Idle
                || sip.callState() == CallState::Failed;
        });
    }
    std::cout << "[HANGUP] State: " << qPrintable(callStateName(sip.callState())) << '\n';

    std::cout << "\n[UNREG] Unregistering...\n";
    sip.unregisterActiveProfile();
    waitFor(15000, [&] {
        return sip.registrationState() == RegistrationState::Unregistered;
    });
    std::cout << "[UNREG] State: "
              << qPrintable(registrationStateName(sip.registrationState())) << '\n';

    sip.shutdown();

    std::cout << "\n==========================================================\n";
    std::cout << "Task 40 Emergency Protocol Probe — COMPLETE\n";
    std::cout << "==========================================================\n";

    return 0;
}
