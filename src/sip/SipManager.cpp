#include "SipManager.h"

#include <QPointer>

#include "core/Logger.h"
#include "core/PerfScope.h"
#include "media/AudioMediaManager.h"
#include "media/MediaDeviceManager.h"
#include "media/MediaDeviceSelectionModel.h"
#include "media/VideoMediaManager.h"
#include "media/VideoQualityManager.h"
#include "security/CredentialStore.h"
#include "sip/CodecManager.h"
#include "sip/PjsipAudioMapper.h"
#include "sip/SipProfileManager.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/RegistrationRefreshConfig.h"
#include "sip/SipTraceLogger.h"
#include "sip/MessageHistoryStore.h"
#include "sip/MessagingContentKind.h"
#include "sip/ImdnParser.h"
#include "sip/IsComposingParser.h"
#include "sip/PresenceInfo.h"
#include "sip/PresenceResubscribePolicy.h"
#include "sip/PresenceStore.h"
#include "core/AppSettings.h"
#include "msrp/MessagingTransportPolicy.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <QHash>
#include "sip/PjsipTraceModule.h"
#endif

#if defined(HAVE_PJSIP) && defined(_WIN32)
#include "media/PjsipGdiRenderer.h"
#endif

#ifdef HAVE_PJSIP

struct SipManager::PjEndpoint {
    pj::Endpoint ep;
    QHash<int, int> transportIds;
};

static bool initPjsip(SipManager::PjEndpoint *&out, QString &errOut)
{
    out = new SipManager::PjEndpoint;
    try {
        out->ep.libCreate();

        pj::EpConfig cfg;
        cfg.logConfig.level = 3;
        cfg.logConfig.consoleLevel = 3;
        out->ep.libInit(cfg);
        out->ep.libStart();

        // Capture full raw SIP messages (request/status line + headers + SDP)
        // for the SIP Ladder detail view — without this, only synthetic
        // per-action summaries are ever logged (see SipManager::makeCall etc.).
        PjsipTraceModule::install();

#if defined(_WIN32)
        {
            pj_status_t st = PjsipGdiRenderer::registerFactory();
            if (st == PJ_SUCCESS) {
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Qt GDI video renderer registered: device index %1")
                        .arg(PjsipGdiRenderer::deviceIndex()));
            } else {
                Logger::instance().warn(LogCategory::Media,
                    QStringLiteral("Qt GDI video renderer registration failed: status=%1").arg(st));
            }
        }
#endif

        return true;
    } catch (const pj::Error &e) {
        errOut = QString::fromStdString(e.reason);
        delete out;
        out = nullptr;
        return false;
    }
}

static void shutdownPjsip(SipManager::PjEndpoint *&ep)
{
    if (!ep)
        return;
    try {
        PjsipTraceModule::uninstall();
        ep->ep.libDestroy();
    } catch (...) {
    }
    delete ep;
    ep = nullptr;
}

static void logVideoDevices()
{
#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("PJSIP video support: ENABLED (PJMEDIA_HAS_VIDEO=1)"));
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Qt GDI renderer device index: %1")
            .arg(PjsipGdiRenderer::deviceIndex()));
    try {
        pj::VidDevManager &vdm = pj::Endpoint::instance().vidDevManager();
        const unsigned count = vdm.getDevCount();
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("PJSIP video devices (%1 total):").arg(count));
        for (unsigned i = 0; i < count; ++i) {
            try {
                pj::VideoDevInfo info = vdm.getDevInfo(static_cast<int>(i));
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("  [%1] \"%2\"  driver=%3  dir=%4")
                        .arg(i)
                        .arg(QString::fromStdString(info.name))
                        .arg(QString::fromStdString(info.driver))
                        .arg(static_cast<int>(info.dir)));
            } catch (...) {}
        }
        if (count == 0) {
            Logger::instance().warn(LogCategory::Media,
                QStringLiteral("PJSIP video: no capture/render devices "
                               "(PJMEDIA_VIDEO_DEV_HAS_DSHOW=0; "
                               "no DirectShow backend compiled)"));
        }
    } catch (const pj::Error &e) {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("PJSIP video device query failed: %1")
                .arg(QString::fromStdString(e.reason)));
    }
#else
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("PJSIP video support: DISABLED (PJMEDIA_HAS_VIDEO=0)"));
#endif
}

#endif

SipManager &SipManager::instance()
{
    static SipManager s_instance;
    return s_instance;
}

SipManager::SipManager() : QObject(nullptr)
{
    qRegisterMetaType<RegistrationState>("RegistrationState");
    qRegisterMetaType<CallState>("CallState");
    qRegisterMetaType<RtpStatsSnapshot>("RtpStatsSnapshot");

    connect(&m_stateMachine, &RegistrationStateMachine::stateChanged,
            this, &SipManager::registrationStateChanged);
    connect(&m_stateMachine, &RegistrationStateMachine::transitionTimedOut,
            this, &SipManager::onStateMachineTimedOut);

    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout,
            this, &SipManager::onRetryTimerFired);

    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout,
            this, &SipManager::onRefreshTimerFired);

    m_rtpStatsTimer.setInterval(1000);
    connect(&m_rtpStatsTimer, &QTimer::timeout,
            this, &SipManager::refreshRtpStats);

    // Re-apply PJSIP audio device selection when the user picks a new device,
    // or when the device list is refreshed (which resolves persisted names).
    connect(&AudioMediaManager::instance(),
            &AudioMediaManager::audioDeviceSelectionChanged,
            this, &SipManager::applyPersistedAudioDevices);
    connect(&MediaDeviceManager::instance(),
            &MediaDeviceManager::devicesChanged,
            this, &SipManager::applyPersistedAudioDevices);

}

SipManager::~SipManager()
{
    if (m_initialized)
        shutdown();
}

bool SipManager::initialize()
{
    if (m_initialized)
        return true;

    Logger::instance().info(LogCategory::Sip, QStringLiteral("Initializing SIP backend"));
    m_lastError.clear();

#ifdef HAVE_PJSIP
    {
        PerfScope s("PJSIP libCreate+libInit+libStart+GDI renderer");
        if (!initPjsip(m_ep, m_lastError)) {
            Logger::instance().error(LogCategory::Sip,
                QStringLiteral("SIP backend initialization failed: %1").arg(m_lastError));
            emit initializationFailed(m_lastError);
            return false;
        }
    }
    { PerfScope s("PjsipAudioMapper::logAllDevices"); PjsipAudioMapper::logAllDevices(); }
    { PerfScope s("logVideoDevices");                 logVideoDevices();                 }
    { PerfScope s("CodecManager::initialize");        CodecManager::instance().initialize(); }
    { PerfScope s("applyPersistedAudioDevices");      applyPersistedAudioDevices();      }
#else
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("PJSIP unavailable - running stub SIP backend"));
#endif

    m_initialized = true;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("SIP backend initialized (%1)").arg(backendName()));
    emit initialized();
    return true;
}

void SipManager::shutdown()
{
    if (!m_initialized)
        return;

    Logger::instance().info(LogCategory::Sip, QStringLiteral("Shutting down SIP backend"));

    m_retryTimer.stop();
    m_retryAttempt = 0;
    m_refreshTimer.stop();
    m_refreshing = false;

    if (m_activeCall) {
        m_activeCall->reset(QStringLiteral("SIP backend shut down"));
        destroyActiveCall();
    }

    if (!m_pendingProfileId.isEmpty()) {
        const QString pending = m_pendingProfileId;
        m_pendingProfileId.clear();
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Profile switch to %1 cancelled by shutdown").arg(pending));
        emit profileSwitchFailed(pending, QStringLiteral("SIP backend shut down"));
    }

    if (m_account) {
#ifdef HAVE_PJSIP
        // Pump the PJSIP event loop briefly so any call-state transitions that
        // were queued just before shutdown (e.g. a failed outgoing call) are
        // fully processed.  Without this, setRegistration(false) can hit
        // PJSIP_EBUSY if the call slot has not been released internally yet.
        if (m_ep) {
            try { m_ep->ep.libHandleEvents(100); } catch (...) {}
        }
#endif
        m_account->startUnregistration();
        destroyAccount();
    }

#ifdef HAVE_PJSIP
    shutdownPjsip(m_ep);
#endif

    m_initialized = false;
    m_stateMachine.reset(QStringLiteral("SIP backend shut down"));
    Logger::instance().info(LogCategory::Sip, QStringLiteral("SIP backend shut down"));
    emit shutdownComplete();
}

bool SipManager::registerActiveProfile()
{
    const RegistrationState current = m_stateMachine.state();

    // Already in progress or completed for the same profile — not an error.
    const QString activeId = SipProfileManager::instance().activeProfileId();
    if ((current == RegistrationState::Registering
         || current == RegistrationState::Registered)
        && m_registeredProfileId == activeId) {
        return true;
    }

    // Reject if the state machine is in a transient state that doesn't allow
    // starting a new registration (Registering different profile, Unregistering).
    if (current == RegistrationState::Registering
        || current == RegistrationState::Unregistering) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Register rejected: current state is %1")
                .arg(registrationStateName(current)));
        return false;
    }

    // Reject if already registered to a different profile (must unregister first).
    if (current == RegistrationState::Registered) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Register rejected: already registered to profile %1; "
                           "call unregisterActiveProfile() first")
                .arg(m_registeredProfileId));
        return false;
    }

    if (!m_initialized && !initialize()) {
        m_stateMachine.tryTransition(RegistrationState::RegistrationFailed,
                                     QStringLiteral("SIP backend initialization failed"));
        return false;
    }

    const SipProfile profile = SipProfileManager::instance().activeProfile();
    if (profile.isNull()) {
        m_stateMachine.tryTransition(RegistrationState::RegistrationFailed,
                                     QStringLiteral("No active SIP profile"));
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("Register failed: no active SIP profile (status 0)"));
        return false;
    }

    if (m_account) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Destroying stale account for profile %1")
                .arg(m_registeredProfileId));
        m_account->startUnregistration();
        destroyAccount();
    }

    const QString authUsername = profile.authUsername.isEmpty()
        ? profile.sipUsername : profile.authUsername;
    bool passwordFound = false;
    const QString password = CredentialStore::instance().loadPassword(
        profile.profileId, authUsername, &passwordFound);
    if (!passwordFound) {
        m_stateMachine.tryTransition(RegistrationState::RegistrationFailed,
                                     QStringLiteral("No credential found for active SIP profile"));
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("Register failed: credential unavailable for profile %1 (status 0)")
                .arg(profile.profileId));
        return false;
    }

#ifdef HAVE_PJSIP
    QString transportError;
    int transportId = -1;
    if (!ensureTransport(profile.transport, transportId, transportError)) {
        m_stateMachine.tryTransition(RegistrationState::RegistrationFailed,
                                     transportError);
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("Register failed: transport creation failed (%1) (status 0)")
                .arg(transportError));
        return false;
    }
#endif

    // Emit REGISTER outbound trace before sending.
    {
        SipMessageTrace trace;
        trace.direction = SipMessageTrace::Direction::Outbound;
        trace.method    = QStringLiteral("REGISTER");
        trace.fromUri   = profile.effectiveSipUri();
        trace.toUri     = QStringLiteral("sip:") + profile.registrar;
        trace.cSeq      = QStringLiteral("1 REGISTER");
        SipTraceLogger::instance().logMessage(trace);
    }

    m_registeredProfileId = profile.profileId;
    m_account = new SipAccount(profile.profileId, this);
    connect(m_account, &SipAccount::registrationStateChanged,
            this, &SipManager::onAccountRegistrationStateChanged);
    connect(m_account, &SipAccount::registrationExpiryReceived,
            this, &SipManager::onAccountRegistrationExpiryReceived);
    connect(m_account, &SipAccount::instantMessageReceived,
            this, &SipManager::onAccountInstantMessageReceived);
    connect(m_account, &SipAccount::instantMessageStatusReceived,
            this, &SipManager::onAccountInstantMessageStatusReceived);
    connect(m_account, &SipAccount::buddyPresenceChanged,
            this, &SipManager::onAccountBuddyPresenceChanged);
#ifdef HAVE_PJSIP
    connect(m_account, &SipAccount::incomingPjsipCallReceived,
            this, [this](const QString &remoteUri, int callId) {
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Incoming PJSIP call dispatch: callId=%1 remote=%2")
                        .arg(callId).arg(remoteUri));
                if (m_activeCall && m_activeCall->state() != CallState::Idle
                                 && m_activeCall->state() != CallState::Failed) {
                    Logger::instance().warn(LogCategory::Sip,
                        QStringLiteral("Incoming PJSIP call rejected locally: a call is already active"));
                    return;
                }

                destroyActiveCall();
                m_activeCall = new SipCall(this);
                m_activeCall->setPjsipAccountHandle(m_account ? m_account->pjAccountHandle() : nullptr);
                wireActiveCall(m_activeCall);
                AudioMediaManager::instance().attachCall(m_activeCall);
                VideoMediaManager::instance().attachCall(m_activeCall);
                m_rttSession.enableForCall(m_activeCall);

                {
                    SipMessageTrace trace;
                    trace.direction = SipMessageTrace::Direction::Inbound;
                    trace.method    = QStringLiteral("INVITE");
                    trace.fromUri   = remoteUri;
                    const SipProfile ip = SipProfileManager::instance().activeProfile();
                    if (!ip.isNull())
                        trace.toUri = ip.effectiveSipUri();
                    SipTraceLogger::instance().logMessage(trace);
                }

                if (m_activeCall->bindIncomingPjsipCall(
                        m_account ? m_account->pjAccountHandle() : nullptr,
                        callId,
                        remoteUri,
                        m_account ? m_account->takeEarlyPjCall(callId) : nullptr)) {
                    emit incomingCall(remoteUri);
                } else {
                    destroyActiveCall();
                }
            });
#else
    connect(m_account, &SipAccount::incomingCallReceived,
            this, &SipManager::onAccountIncomingCall);
#endif

    m_stateMachine.tryTransition(RegistrationState::Registering,
                                 QStringLiteral("Registration started"));
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Register started for profile %1").arg(profile.profileId));

#ifdef HAVE_PJSIP
    return m_account->startRegistration(profile, password, transportId);
#else
    return m_account->startRegistration(profile, password);
#endif
}

bool SipManager::unregisterActiveProfile()
{
    m_retryTimer.stop();
    m_retryAttempt = 0;
    m_refreshTimer.stop();
    m_refreshing = false;

    const RegistrationState current = m_stateMachine.state();

    if (current == RegistrationState::Unregistered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Unregister: already in Unregistered state — no-op"));
        return true;
    }

    if (current == RegistrationState::Unregistering) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Unregister: already in Unregistering state — no-op"));
        return true;
    }

    if (!m_account) {
        // RegistrationFailed with no account — just reset to Unregistered.
        m_stateMachine.tryTransition(RegistrationState::Unregistered,
                                     QStringLiteral("Unregistered (no account)"));
        return true;
    }

    if (current == RegistrationState::Registering) {
        // Cancel an in-flight registration.
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Unregister: cancelling in-flight registration for profile %1")
                .arg(m_registeredProfileId));
        m_stateMachine.tryTransition(RegistrationState::Unregistering,
                                     QStringLiteral("Cancelling registration"));
        return m_account->startUnregistration();
    }

    // Registered or RegistrationFailed with an account.
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Unregister started for profile %1").arg(m_registeredProfileId));

    {
        SipMessageTrace trace;
        trace.direction = SipMessageTrace::Direction::Outbound;
        trace.method    = QStringLiteral("REGISTER"); // SIP uses REGISTER with Expires:0
        trace.cSeq      = QStringLiteral("2 REGISTER");
        const SipProfile up = SipProfileManager::instance().activeProfile();
        if (!up.isNull()) {
            trace.fromUri = up.effectiveSipUri();
            trace.toUri   = QStringLiteral("sip:") + up.registrar;
        }
        SipTraceLogger::instance().logMessage(trace);
    }

    m_stateMachine.tryTransition(RegistrationState::Unregistering,
                                 QStringLiteral("Unregistering"));
    return m_account->startUnregistration();
}

bool SipManager::isInitialized() const
{
    return m_initialized;
}

bool SipManager::isPjsipAvailable() const
{
#ifdef HAVE_PJSIP
    return true;
#else
    return false;
#endif
}

bool SipManager::hasPjsipVideoCapture() const
{
#if defined(HAVE_PJSIP) && defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO
    if (!m_ep || !m_initialized)
        return false;
    try {
        pj::VidDevManager &vdm = pj::Endpoint::instance().vidDevManager();
        const unsigned count = vdm.getDevCount();
        for (unsigned i = 0; i < count; ++i) {
            try {
                pj::VideoDevInfo info = vdm.getDevInfo(static_cast<int>(i));
                if (info.dir == PJMEDIA_DIR_CAPTURE
                        || info.dir == PJMEDIA_DIR_CAPTURE_RENDER)
                    return true;
            } catch (...) {}
        }
    } catch (...) {}
    return false;
#else
    return false;
#endif
}

QString SipManager::backendName() const
{
#ifdef HAVE_PJSIP
    return QStringLiteral("PJSIP/pjsua2");
#else
    return QStringLiteral("Stub SIP backend");
#endif
}

QString SipManager::lastError() const
{
    return m_lastError;
}

RegistrationState SipManager::registrationState() const
{
    return m_stateMachine.state();
}

QString SipManager::registrationStatusText() const
{
    if (!m_pendingProfileId.isEmpty())
        return QStringLiteral("Switching SIP profile...");
    if (m_refreshing && m_stateMachine.state() == RegistrationState::Registered)
        return QStringLiteral("Registered (refreshing...)");
    return m_stateMachine.statusText();
}

int SipManager::registrationStatusCode() const
{
    return m_stateMachine.statusCode();
}

QString SipManager::registeredProfileId() const
{
    return m_registeredProfileId;
}

RegistrationStateMachine &SipManager::stateMachine()
{
    return m_stateMachine;
}

bool SipManager::switchActiveProfile(const QString &newProfileId)
{
    if (!m_pendingProfileId.isEmpty()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Profile switch to %1 rejected: switch to %2 already pending")
                .arg(newProfileId, m_pendingProfileId));
        return false;
    }

    const RegistrationState current = m_stateMachine.state();
    const QString oldId = m_registeredProfileId.isEmpty()
        ? SipProfileManager::instance().activeProfileId()
        : m_registeredProfileId;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile switch requested: %1 -> %2 (state: %3)")
            .arg(oldId.isEmpty() ? QStringLiteral("(none)") : oldId,
                 newProfileId.isEmpty() ? QStringLiteral("(none)") : newProfileId,
                 registrationStateName(current)));

    if (current == RegistrationState::Unregistered
        || current == RegistrationState::RegistrationFailed) {
        // Fast path: no active registration to tear down.
        m_retryTimer.stop();
        m_retryAttempt = 0;
        m_refreshTimer.stop();
        m_refreshing = false;
        if (newProfileId.isEmpty()) {
            destroyAccount();
            SipProfileManager::instance().setActiveProfileId({});
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Profile switch complete: switched to (no profile)"));
            emit profileSwitchCompleted({});
        } else {
            SipProfileManager::instance().setActiveProfileId(newProfileId);
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Profile switch: registering new profile %1").arg(newProfileId));
            registerActiveProfile();
            emit profileSwitchCompleted(newProfileId);
        }
        return true;
    }

    // Slow path: active account must be unregistered before the new profile is created.
    m_pendingProfileId = newProfileId;
    m_retryTimer.stop();
    m_retryAttempt = 0;
    m_refreshTimer.stop();
    m_refreshing = false;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile switch: unregistering profile %1 before switching to %2")
            .arg(oldId, newProfileId.isEmpty() ? QStringLiteral("(none)") : newProfileId));
    emit profileSwitchStarted(newProfileId);
    unregisterActiveProfile();
    return true;
}

bool SipManager::isSwitchingProfile() const
{
    return !m_pendingProfileId.isEmpty();
}

void SipManager::completePendingSwitch()
{
    const QString newId = m_pendingProfileId;
    m_pendingProfileId.clear();
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile switch: old account destroyed, completing switch to %1")
            .arg(newId.isEmpty() ? QStringLiteral("(none)") : newId));
    SipProfileManager::instance().setActiveProfileId(newId);
    if (!newId.isEmpty()) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Profile switch: starting registration for new profile %1").arg(newId));
        registerActiveProfile();
    }
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile switch complete: now using profile %1")
            .arg(newId.isEmpty() ? QStringLiteral("(none)") : newId));
    emit profileSwitchCompleted(newId);
}

void SipManager::onAccountRegistrationExpiryReceived(int seconds)
{
    m_registrationExpirySeconds = seconds;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Registration expiry received: %1 s for profile %2")
            .arg(seconds).arg(m_registeredProfileId));
    emit registrationExpiryChanged(seconds);
}

void SipManager::onAccountRegistrationStateChanged(RegistrationState state,
                                                   const QString &statusText,
                                                   int statusCode)
{
    if (m_refreshing) {
        m_refreshing = false;

        if (state == RegistrationState::Registered) {
            // Refresh success — state stays Registered, just restart the timer.
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Registration refresh succeeded for profile %1 (status %2)")
                    .arg(m_registeredProfileId).arg(statusCode));
            m_retryAttempt = 0;
            scheduleRefresh(m_registrationExpirySeconds);
            return;
        }

        if (state == RegistrationState::RegistrationFailed) {
            Logger::instance().error(LogCategory::Sip,
                QStringLiteral("Registration refresh failed for profile %1: %2 (status %3)")
                    .arg(m_registeredProfileId, statusText).arg(statusCode));
            // Registered → RegistrationFailed is now a valid SM transition (refresh failure).
            m_stateMachine.tryTransition(RegistrationState::RegistrationFailed,
                                         statusText, statusCode);
            scheduleRetryIfEligible(statusCode);
            return;
        }
    }

    m_stateMachine.tryTransition(state, statusText, statusCode);

    if (state == RegistrationState::Registered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Register success for profile %1 (status %2)")
                .arg(m_registeredProfileId).arg(statusCode));
        {
            SipMessageTrace trace;
            trace.direction  = SipMessageTrace::Direction::Inbound;
            trace.statusCode = (statusCode > 0) ? statusCode : 200;
            trace.statusText = QStringLiteral("OK");
            trace.method     = QStringLiteral("REGISTER");
            trace.cSeq       = QStringLiteral("1 REGISTER");
            const SipProfile rp = SipProfileManager::instance().activeProfile();
            if (!rp.isNull()) {
                trace.fromUri = QStringLiteral("sip:") + rp.registrar;
                trace.toUri   = rp.effectiveSipUri();
            }
            SipTraceLogger::instance().logMessage(trace);
        }
        m_retryAttempt = 0;
        scheduleRefresh(m_registrationExpirySeconds);
    } else if (state == RegistrationState::RegistrationFailed) {
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("Register failed for profile %1: %2 (status %3)")
                .arg(m_registeredProfileId, statusText).arg(statusCode));
        {
            SipMessageTrace trace;
            trace.direction  = SipMessageTrace::Direction::Inbound;
            trace.statusCode = (statusCode > 0) ? statusCode : 503;
            trace.statusText = statusText.isEmpty()
                ? QStringLiteral("Service Unavailable") : statusText;
            trace.method     = QStringLiteral("REGISTER");
            trace.cSeq       = QStringLiteral("1 REGISTER");
            const SipProfile fp = SipProfileManager::instance().activeProfile();
            if (!fp.isNull()) {
                trace.fromUri = QStringLiteral("sip:") + fp.registrar;
                trace.toUri   = fp.effectiveSipUri();
            }
            SipTraceLogger::instance().logMessage(trace);
        }
        scheduleRetryIfEligible(statusCode);
    } else if (state == RegistrationState::Unregistered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Unregister success for profile %1 (status %2)")
                .arg(m_registeredProfileId).arg(statusCode));
        {
            SipMessageTrace trace;
            trace.direction  = SipMessageTrace::Direction::Inbound;
            trace.statusCode = (statusCode > 0) ? statusCode : 200;
            trace.statusText = QStringLiteral("OK");
            trace.method     = QStringLiteral("REGISTER");
            trace.cSeq       = QStringLiteral("2 REGISTER");
            const SipProfile up = SipProfileManager::instance().activeProfile();
            if (!up.isNull()) {
                trace.fromUri = QStringLiteral("sip:") + up.registrar;
                trace.toUri   = up.effectiveSipUri();
            }
            SipTraceLogger::instance().logMessage(trace);
        }
        destroyAccount();
        if (!m_pendingProfileId.isEmpty())
            completePendingSwitch();
    }
}

void SipManager::onStateMachineTimedOut(RegistrationState stuckState)
{
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("Registration state machine timed out in %1; cleaning up account")
            .arg(registrationStateName(stuckState)));
    destroyAccount();

    if (!m_pendingProfileId.isEmpty()) {
        // Timeout during a profile switch — destroy old account and proceed anyway.
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Timeout during profile switch; proceeding with switch to %1")
                .arg(m_pendingProfileId));
        completePendingSwitch();
        return;
    }

    // Watchdog timeout during Registering is treated as a transient failure (code 0).
    if (stuckState == RegistrationState::Registering)
        scheduleRetryIfEligible(0);
}

void SipManager::destroyAccount()
{
    if (!m_account)
        return;
    disconnect(m_account, nullptr, this, nullptr);
    delete m_account;
    m_account = nullptr;
    m_registeredProfileId.clear();

    // Any live presence subscriptions belonged to the destroyed account's
    // Buddy objects — stop pending backoff retries so a resubscribe attempt
    // never fires against a URI whose subscription no longer exists.
    for (QTimer *timer : qAsConst(m_presenceBackoffTimers))
        delete timer;
    m_presenceBackoffTimers.clear();
    m_presenceBackoffAttempts.clear();
}

void SipManager::scheduleRetryIfEligible(int statusCode)
{
    if (!RegistrationRetryPolicy::isRetryable(statusCode)) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Registration failed with non-retryable status %1; not retrying")
                .arg(statusCode));
        m_retryAttempt = 0;
        return;
    }

    if (m_retryAttempt >= m_retryPolicy.maxAttempts) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Max retry attempts (%1) reached for profile %2; giving up")
                .arg(m_retryPolicy.maxAttempts)
                .arg(SipProfileManager::instance().activeProfileId()));
        m_retryAttempt = 0;
        return;
    }

    ++m_retryAttempt;
    const int delay = m_retryPolicy.delayForAttempt(m_retryAttempt);
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Scheduling retry %1/%2 in %3 ms for profile %4")
            .arg(m_retryAttempt)
            .arg(m_retryPolicy.maxAttempts)
            .arg(delay)
            .arg(SipProfileManager::instance().activeProfileId()));
    m_retryTimer.start(delay);
    emit retryScheduled(m_retryAttempt, delay);
}

void SipManager::setRetryPolicy(const RegistrationRetryPolicy &policy)
{
    m_retryPolicy = policy;
}

const RegistrationRetryPolicy &SipManager::retryPolicy() const
{
    return m_retryPolicy;
}

int SipManager::retryAttempt() const
{
    return m_retryAttempt;
}

void SipManager::onRetryTimerFired()
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Retry attempt %1/%2 for profile %3")
            .arg(m_retryAttempt)
            .arg(m_retryPolicy.maxAttempts)
            .arg(SipProfileManager::instance().activeProfileId()));
    registerActiveProfile();
}

void SipManager::scheduleRefresh(int expirySeconds)
{
    m_refreshTimer.stop();
    m_registrationExpirySeconds = (expirySeconds > 0)
        ? expirySeconds
        : m_refreshConfig.defaultExpirySeconds;
    const int delayMs = m_refreshConfig.delayMsForExpiry(m_registrationExpirySeconds);
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Registration refresh scheduled in %1 ms (expiry %2 s) for profile %3")
            .arg(delayMs)
            .arg(m_registrationExpirySeconds)
            .arg(SipProfileManager::instance().activeProfileId()));
    m_refreshTimer.start(delayMs);
    emit refreshScheduled(delayMs);
}

void SipManager::onRefreshTimerFired()
{
    emit refreshStarted();
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Triggering registration refresh for profile %1")
            .arg(SipProfileManager::instance().activeProfileId()));

    if (!m_account) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Refresh fired but no active account; falling back to retry"));
        scheduleRetryIfEligible(0);
        return;
    }

    m_refreshing = true;
    m_account->refreshRegistration();
}

// ---------------------------------------------------------------------------
// Call control
// ---------------------------------------------------------------------------

bool SipManager::setCallMuted(bool muted)
{
    if (!m_activeCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("setCallMuted: no active call"));
        return false;
    }
    return m_activeCall->setMuted(muted);
}

bool SipManager::isCallMuted() const
{
    return m_activeCall ? m_activeCall->isMuted() : false;
}

bool SipManager::setCallVideoMuted(bool muted)
{
    if (!m_activeCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("setCallVideoMuted: no active call"));
        return false;
    }
    const CallState st = callState();
    const bool inCall  = (st == CallState::Active || st == CallState::Held);
    if (muted) {
        if (inCall) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Camera Off requested during active call"));
            if (m_activeCall->isLocalVideoAvailable())
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Stopping local video transmit"));
        }
    } else {
        if (inCall) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Camera On requested during active call"));
            if (m_activeCall->isLocalVideoAvailable())
                Logger::instance().info(LogCategory::Sip,
                    QStringLiteral("Starting local video transmit"));
        }
    }
    const bool ok = m_activeCall->setVideoMuted(muted);
    if (ok && inCall && m_activeCall->isLocalVideoAvailable()) {
        Logger::instance().info(LogCategory::Sip,
            muted ? QStringLiteral("Local video transmit stopped")
                  : QStringLiteral("Local video transmit started"));
    }
    return ok;
}

bool SipManager::isCallVideoMuted() const
{
    return m_activeCall ? m_activeCall->isVideoMuted() : false;
}

bool SipManager::requestCallVideo(bool enabled)
{
    if (!m_activeCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("requestCallVideo: no active call"));
        return false;
    }
    if (enabled) {
        // Audio-only calls may have zeroed the video codec priorities during
        // makeCall(); restore the current video profile before renegotiating.
        applyVideoSettingsForCall();
    }
    return m_activeCall->requestVideo(enabled);
}

bool SipManager::requestCallRtt(bool enabled)
{
    if (!m_activeCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("requestCallRtt: no active call"));
        return false;
    }
    return m_activeCall->requestRtt(enabled);
}

void SipManager::sendRttText(const QString &text)
{
    if (!m_activeCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendRttText: no active call"));
        return;
    }
    m_rttSession.sendText(text);
}

RttSession *SipManager::rttSession()
{
    return &m_rttSession;
}

RtpStatsSnapshot SipManager::currentRtpStats() const
{
    if (!m_activeCall) {
        RtpStatsSnapshot snap;
        snap.reason = QStringLiteral("No active call");
        return snap;
    }
    const CallState state = m_activeCall->state();
    if (state == CallState::Disconnecting
        || state == CallState::Idle
        || state == CallState::Failed) {
        RtpStatsSnapshot snap;
        snap.reason = QStringLiteral("Call teardown in progress");
        return snap;
    }
    return m_activeCall->mediaRtpStats();
}

AudioCodecInfo SipManager::activeAudioCodecInfo() const
{
    if (!m_activeCall)
        return {};
    return m_activeCall->negotiatedAudioCodecInfo();
}

VideoCodecInfo SipManager::activeVideoCodecInfo() const
{
    if (!m_activeCall)
        return {};
    return m_activeCall->negotiatedVideoCodecInfo();
}

bool SipManager::sendEmergencyLocationUpdate(const SipCallOptions &opts)
{
    if (!opts.emergencyCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendEmergencyLocationUpdate rejected: "
                           "opts.emergencyCall is false — not an emergency update"));
        return false;
    }
    if (!m_activeCall) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendEmergencyLocationUpdate rejected: no active call"));
        return false;
    }
    return m_activeCall->sendLocationUpdate(opts);
}

void SipManager::destroyActiveCall()
{
    if (!m_activeCall)
        return;
    m_rtpStatsTimer.stop();
    m_rttSession.disable();
    AudioMediaManager::instance().detachCall();
    VideoMediaManager::instance().detachCall();
    disconnect(m_activeCall, nullptr, this, nullptr);
    delete m_activeCall;
    m_activeCall = nullptr;
    emit rtpStatsChanged(currentRtpStats());
}

void SipManager::refreshRtpStats()
{
    emit rtpStatsChanged(currentRtpStats());
}

void SipManager::wireActiveCall(SipCall *call)
{
    if (!call)
        return;

    connect(call, &SipCall::callStateChanged,
            this, &SipManager::onActiveCallStateChanged);
    connect(call, &SipCall::callConnected,
            this, &SipManager::callConnected);
    connect(call, &SipCall::callDisconnected,
            this, &SipManager::callDisconnected);
    connect(call, &SipCall::callFailed,
            this, &SipManager::callFailed);
    connect(call, &SipCall::audioMediaConnected,
            this, &SipManager::audioMediaConnected);
    connect(call, &SipCall::audioMediaDisconnected,
            this, &SipManager::audioMediaDisconnected);
    connect(call, &SipCall::muteChanged,
            this, &SipManager::callMuteChanged);
    connect(call, &SipCall::inputLevelChanged,
            this, &SipManager::callInputLevelChanged);
    connect(call, &SipCall::outputLevelChanged,
            this, &SipManager::callOutputLevelChanged);
    connect(call, &SipCall::videoMediaConnected,
            this, &SipManager::videoMediaConnected);
    connect(call, &SipCall::videoMediaDisconnected,
            this, &SipManager::videoMediaDisconnected);
    connect(call, &SipCall::videoRequested,
            this, &SipManager::videoRequested);
    connect(call, &SipCall::rttRequested,
            this, &SipManager::rttRequested);
    connect(call, &SipCall::videoMuteChanged,
            this, &SipManager::callVideoMuteChanged);
    connect(call, &SipCall::localVideoStarted,
            this, &SipManager::localVideoStarted);
    connect(call, &SipCall::localVideoStopped,
            this, &SipManager::localVideoStopped);
    connect(call, &SipCall::remoteVideoStarted,
            this, &SipManager::remoteVideoStarted);
    connect(call, &SipCall::remoteVideoStopped,
            this, &SipManager::remoteVideoStopped);
    connect(call, &SipCall::rttMediaConnected,
            this, &SipManager::rttMediaConnected);
    connect(call, &SipCall::rttMediaDisconnected,
            this, &SipManager::rttMediaDisconnected);
    connect(call, &SipCall::rttTextReceived,
            this, &SipManager::rttTextReceived);
    connect(call, &SipCall::callStateChanged,
            this, &SipManager::refreshRtpStats);
    connect(call, &SipCall::audioMediaConnected,
            this, &SipManager::refreshRtpStats);
    connect(call, &SipCall::audioMediaDisconnected,
            this, &SipManager::refreshRtpStats);
    connect(call, &SipCall::videoMediaConnected,
            this, &SipManager::refreshRtpStats);
    connect(call, &SipCall::videoMediaDisconnected,
            this, &SipManager::refreshRtpStats);
    connect(call, &SipCall::rttMediaConnected,
            this, &SipManager::refreshRtpStats);
    connect(call, &SipCall::rttMediaDisconnected,
            this, &SipManager::refreshRtpStats);

    if (!m_rtpStatsTimer.isActive())
        m_rtpStatsTimer.start();
    refreshRtpStats();
}

bool SipManager::prepareOutgoingCall(const QString &remoteUri)
{
    if (m_activeCall && m_activeCall->state() != CallState::Idle
                     && m_activeCall->state() != CallState::Failed) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("makeCall rejected: a call is already active (state: %1)")
                .arg(callStateName(m_activeCall->state())));
        return false;
    }

    if (remoteUri.trimmed().isEmpty()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("makeCall rejected: empty remote URI"));
        return false;
    }

    destroyActiveCall();
    m_activeCall = new SipCall(this);
    wireActiveCall(m_activeCall);
    AudioMediaManager::instance().attachCall(m_activeCall);
    VideoMediaManager::instance().attachCall(m_activeCall);
    m_rttSession.enableForCall(m_activeCall);
#ifdef HAVE_PJSIP
    if (m_account)
        m_activeCall->setPjsipAccountHandle(m_account->pjAccountHandle());
#endif
    return true;
}

bool SipManager::makeCall(const QString &remoteUri)
{
    if (!prepareOutgoingCall(remoteUri))
        return false;

    applyVideoSettingsForCall();

    // Emit INVITE outbound trace.
    {
        SipMessageTrace trace;
        trace.direction = SipMessageTrace::Direction::Outbound;
        trace.method    = QStringLiteral("INVITE");
        trace.toUri     = remoteUri.trimmed();
        const SipProfile cp = SipProfileManager::instance().activeProfile();
        if (!cp.isNull())
            trace.fromUri = cp.effectiveSipUri();
        SipTraceLogger::instance().logMessage(trace);
    }

    return m_activeCall->makeCall(remoteUri);
}

bool SipManager::makeCall(const QString &remoteUri, const CallMediaOptions &opts)
{
    if (!prepareOutgoingCall(remoteUri))
        return false;

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("makeCall with CallType=%1 (audio=%2 video=%3 rtt=%4)")
            .arg(callTypeName(opts.type))
            .arg(opts.enableAudio).arg(opts.enableVideo).arg(opts.enableRtt));

    // For audio-only calls disable all video codecs; otherwise apply normal order.
    if (!opts.enableVideo) {
        CodecManager::instance().applyVideoCodecOrder(QStringList{});
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("makeCall: video disabled — all video codec priorities set to 0"));
    } else {
        applyVideoSettingsForCall();
    }

    // Map the selected call type onto the per-call SIP options so the SDP
    // offer matches what the user picked (audio-only by default).
    SipCallOptions callOpts;
    callOpts.requireAudio = opts.enableAudio;
    callOpts.allowVideo   = opts.enableVideo;
    callOpts.requireRtt   = opts.enableRtt;

    // Emit INVITE outbound trace.
    {
        SipMessageTrace trace;
        trace.direction = SipMessageTrace::Direction::Outbound;
        trace.method    = QStringLiteral("INVITE");
        trace.toUri     = remoteUri.trimmed();
        const SipProfile cp = SipProfileManager::instance().activeProfile();
        if (!cp.isNull())
            trace.fromUri = cp.effectiveSipUri();
        SipTraceLogger::instance().logMessage(trace);
    }

    return m_activeCall->makeCallWithOptions(remoteUri, callOpts);
}

bool SipManager::makeEmergencyCall(const QString &remoteUri, const SipCallOptions &options)
{
    if (!prepareOutgoingCall(remoteUri))
        return false;

    applyVideoSettingsForCall();

    // Emit INVITE outbound trace (emergency).
    {
        SipMessageTrace trace;
        trace.direction = SipMessageTrace::Direction::Outbound;
        trace.method    = QStringLiteral("INVITE");
        trace.toUri     = remoteUri.trimmed();
        const SipProfile cp = SipProfileManager::instance().activeProfile();
        if (!cp.isNull())
            trace.fromUri = cp.effectiveSipUri();
        SipTraceLogger::instance().logMessage(trace);
    }

    return m_activeCall->makeCallWithOptions(remoteUri, options);
}

bool SipManager::sendSipMessage(const ComposedSipMessage &msg, QString &error)
{
    if (!msg.valid) {
        error = msg.error.isEmpty() ? QStringLiteral("Message was not composed") : msg.error;
        return false;
    }

    // Emit the outbound trace unconditionally (mirrors makeCall's INVITE
    // trace) so the attempt is visible in the SIP Ladder / Messaging
    // Diagnostics regardless of whether the underlying send succeeds. This
    // feeds MessagingEventStore exactly as before (Task W092) — the
    // Message History entry appended below is a separate store, so the
    // same message is never logged twice into the same list.
    {
        SipMessageTrace trace;
        trace.direction   = SipMessageTrace::Direction::Outbound;
        trace.method      = QStringLiteral("MESSAGE");
        trace.fromUri     = msg.fromUri;
        trace.toUri       = msg.toUri;
        trace.callId      = msg.callId;
        trace.cSeq        = msg.cSeq;
        trace.contentType = msg.contentType;
        trace.rawSip      = msg.rawSip;
        SipTraceLogger::instance().logMessage(trace);
    }

    const qint64 historyId = MessageHistoryStore::instance().appendOutbound(msg);

    if (!m_account) {
        error = QStringLiteral("No active SIP account; register first");
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendSipMessage rejected: no active account"));
        MessageHistoryStore::instance().updateOutboundStatus(
            historyId, MessageHistoryEntry::OutboundStatus::Failed);
        return false;
    }

    // Task W101 Phase 5: route through MessagingTransportPolicy instead of
    // always composing a SIP MESSAGE. "MSRP established" means this specific
    // call's own MsrpSession has actually reached Established — never
    // inferred from SDP negotiation alone. Single active-call model, so the
    // only candidate session is m_activeCall, and only when it's actually
    // talking to this message's peer (never assumed from IP/port).
    const MessagingTransportMode mode = messagingTransportModeFromString(
        AppSettings::messagingTransportMode());
    const bool allowFallback = AppSettings::allowSipMessageFallback();
    const bool msrpCandidate = m_activeCall
        && m_activeCall->isMsrpEstablished()
        && m_activeCall->remoteUri().compare(msg.toUri, Qt::CaseInsensitive) == 0;

    const MessagingTransportPolicy::Decision decision =
        MessagingTransportPolicy::decideInitialTransport(mode, msrpCandidate, allowFallback);

    if (!decision.allowed) {
        error = decision.reason;
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("sendSipMessage rejected by transport policy: %1").arg(decision.reason));
        MessageHistoryStore::instance().updateOutboundStatus(
            historyId, MessageHistoryEntry::OutboundStatus::Failed);
        return false;
    }

    if (decision.transport == MessagingActualTransport::Msrp) {
        const QString msrpMessageId = m_activeCall->sendMsrpMessage(msg.contentType, msg.body.toUtf8());
        if (!msrpMessageId.isEmpty()) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Message sent via MSRP: msrpMessageId=%1").arg(msrpMessageId));
            // "Submitted": MSRP SEND has gone out; the 200/REPORT that
            // upgrades this to Sent/Failed arrives asynchronously via
            // MsrpSession's own transaction tracking (Phase 6).
            MessageHistoryStore::instance().updateOutboundStatus(
                historyId, MessageHistoryEntry::OutboundStatus::Submitted);
            return true;
        }
        // MSRP send attempt failed (e.g. session dropped between the
        // established check and the send) — decide whether a fallback SIP
        // MESSAGE is permitted; never both (no dual-send).
        const MessagingTransportPolicy::Decision fallback =
            MessagingTransportPolicy::decideFallbackAfterMsrpFailure(mode, allowFallback);
        if (!fallback.allowed) {
            error = fallback.reason;
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("MSRP send failed and no fallback permitted: %1").arg(fallback.reason));
            MessageHistoryStore::instance().updateOutboundStatus(
                historyId, MessageHistoryEntry::OutboundStatus::Failed);
            return false;
        }
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("MSRP send failed; falling back to SIP MESSAGE"));
    }

    const bool ok = m_account->sendMessage(msg.toUri, msg.contentType, msg.body,
                                           msg.extraHeaders, historyId, error);
    // "Submitted" (not "Sent"/"delivered"): pjsua2 has only accepted the
    // request for transmission at this point. If the account later reports
    // a final SIP response via onInstantMessageStatus, this status is
    // upgraded to Sent/Failed in onAccountInstantMessageStatusReceived. In
    // stub mode (or if PJSIP never confirms), it permanently stays at
    // Submitted or Failed — never silently reported as delivered.
    MessageHistoryStore::instance().updateOutboundStatus(
        historyId, ok ? MessageHistoryEntry::OutboundStatus::Submitted
                       : MessageHistoryEntry::OutboundStatus::Failed);
    return ok;
}

void SipManager::onAccountInstantMessageReceived(const QString &fromUri, const QString &toUri,
                                                 const QString &contactUri, const QString &contentType,
                                                 const QString &body, const QString &callId,
                                                 const QString &profileId, const QString &messageId,
                                                 const QString &dispositionNotification)
{
    // Deliberately does NOT touch SipTraceLogger/MessagingEventStore — the
    // same inbound MESSAGE is already captured independently by
    // PjsipTraceModule's raw-trace tap (Task W090), which is the sole
    // source for the SIP Ladder / Messaging Diagnostics feed. Routing this
    // callback into MessageHistoryStore only is what avoids the duplicate.

    // Task W096: an incoming message/imdn+xml body is itself an IMDN
    // report — parse it, log it as its own history row, and correlate its
    // disposition against the *outbound* entry it refers to. It never
    // triggers a further IMDN of its own (RFC 5438 reports are not
    // acknowledged).
    if (MessagingContentKindDetector::detect(contentType) == MessagingContentKind::Imdn) {
        const ImdnInfo info = ImdnParser::parse(body);
        MessageHistoryStore::instance().appendInboundImdn(
            fromUri, toUri, contactUri, body, callId, profileId, info.messageId);
        if (info.present && !info.messageId.isEmpty()) {
            MessageHistoryEntry::DeliveryState state = MessageHistoryEntry::DeliveryState::None;
            switch (info.disposition) {
            case ImdnInfo::Disposition::Delivered: state = MessageHistoryEntry::DeliveryState::Delivered; break;
            case ImdnInfo::Disposition::Displayed: state = MessageHistoryEntry::DeliveryState::Displayed; break;
            case ImdnInfo::Disposition::Failed:
            case ImdnInfo::Disposition::Forbidden:
                state = MessageHistoryEntry::DeliveryState::Failed; break;
            case ImdnInfo::Disposition::Error:     state = MessageHistoryEntry::DeliveryState::Error; break;
            default: break;
            }
            if (state != MessageHistoryEntry::DeliveryState::None)
                MessageHistoryStore::instance().correlateDelivery(info.messageId, state);
        }
        return;
    }

    // Task W097: an incoming application/im-iscomposing+xml body is a
    // typing-state notification, not a chat message — log it as its own
    // history row (MessagingEventStore/Messaging Diagnostics already
    // receive it unchanged via the independent raw-trace pipeline, Task
    // W090, so nothing further is duplicated here).
    if (MessagingContentKindDetector::detect(contentType) == MessagingContentKind::IsComposing) {
        const IsComposingInfo info = IsComposingParser::parse(body);
        MessageHistoryStore::instance().appendInboundTyping(
            fromUri, toUri, contactUri, body, callId, profileId,
            IsComposingInfo::stateToString(info.state));
        return;
    }

    const qint64 entryId = MessageHistoryStore::instance().appendInbound(
        fromUri, toUri, contactUri, contentType, body, callId, profileId,
        messageId, dispositionNotification);

    if (messageId.trimmed().isEmpty())
        return;

    const bool wantsDelivered =
        dispositionNotification.contains(QStringLiteral("positive-delivery"), Qt::CaseInsensitive);
    const bool wantsDisplayed =
        dispositionNotification.contains(QStringLiteral("positive-display"), Qt::CaseInsensitive);

    if (wantsDelivered && AppSettings::autoSendDeliveredImdn()) {
        QString error;
        sendImdnReport(fromUri, messageId, ImdnInfo::Disposition::Delivered, entryId, error);
    }
    if (wantsDisplayed && AppSettings::autoSendDisplayedImdn()) {
        QString error;
        sendImdnReport(fromUri, messageId, ImdnInfo::Disposition::Displayed, entryId, error);
    }
}

bool SipManager::sendImdnReport(const QString &toUri, const QString &originalMessageId,
                                ImdnInfo::Disposition disposition, qint64 inboundEntryId,
                                QString &error)
{
    SipMessageComposer::ImdnReportOptions opts;
    opts.toUri = toUri;
    const SipProfile cp = SipProfileManager::instance().activeProfile();
    opts.fromUri = cp.isNull() ? QString() : cp.effectiveSipUri();
    opts.originalMessageId = originalMessageId;
    opts.disposition = disposition;

    const ComposedSipMessage composed = SipMessageComposer::composeImdnReport(opts);
    if (!composed.valid) {
        error = composed.error;
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("IMDN report not sent: %1").arg(error));
        return false;
    }

    const bool ok = sendSipMessage(composed, error);
    if (ok)
        MessageHistoryStore::instance().markImdnSent(inboundEntryId, disposition);
    return ok;
}

bool SipManager::sendDisplayedImdnForEntry(qint64 inboundEntryId, QString &error)
{
    const MessageHistoryEntry entry = MessageHistoryStore::instance().entryById(inboundEntryId);
    if (entry.id == 0 || entry.direction != MessageHistoryEntry::Direction::Inbound) {
        error = QStringLiteral("Message History entry not found");
        return false;
    }
    if (entry.messageId.trimmed().isEmpty()) {
        error = QStringLiteral("Message had no Message-ID; cannot correlate a Displayed report");
        return false;
    }
    if (entry.displayedImdnSent) {
        error = QStringLiteral("Displayed report already sent for this message");
        return false;
    }
    return sendImdnReport(entry.peerUri, entry.messageId, ImdnInfo::Disposition::Displayed,
                          entry.id, error);
}

void SipManager::onAccountInstantMessageStatusReceived(qint64 correlationId, bool success,
                                                        int /*statusCode*/, const QString & /*reason*/)
{
    MessageHistoryStore::instance().updateOutboundStatus(
        correlationId, success ? MessageHistoryEntry::OutboundStatus::Sent
                               : MessageHistoryEntry::OutboundStatus::Failed);
}

bool SipManager::subscribePresence(const QString &targetUri, QString &error)
{
    if (!AppSettings::enablePresence() || !AppSettings::enablePresenceSubscribe()) {
        error = QStringLiteral("Presence subscribe is disabled in settings");
        return false;
    }
    if (targetUri.trimmed().isEmpty()) {
        error = QStringLiteral("Target SIP URI is empty");
        return false;
    }
    if (!m_account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }
    return m_account->subscribePresence(targetUri, error);
}

bool SipManager::unsubscribePresence(const QString &targetUri, QString &error)
{
    if (!m_account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }
    // Cancel any pending auto-resubscribe for this entity — an explicit
    // Unsubscribe is a user action that must win over a scheduled retry.
    if (QTimer *timer = m_presenceBackoffTimers.value(targetUri))
        timer->stop();
    m_presenceBackoffAttempts.remove(targetUri);
    return m_account->unsubscribePresence(targetUri, error);
}

bool SipManager::refreshPresenceSubscription(const QString &targetUri, QString &error)
{
    if (!m_account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }
    return m_account->refreshPresenceSubscription(targetUri, error);
}

bool SipManager::setOwnPresenceState(const QString &basicStatus, const QString &activity,
                                     const QString &note, QString &error)
{
    if (!AppSettings::enablePresence()) {
        error = QStringLiteral("Presence is disabled in settings");
        return false;
    }
    if (!m_account) {
        error = QStringLiteral("No active SIP account");
        return false;
    }
    return m_account->setOwnPresenceState(basicStatus, activity, note, error);
}

void SipManager::onAccountBuddyPresenceChanged(const QString &entityUri, const QString &contactUri,
                                               const QString &basicStatus, const QString &activity,
                                               const QString &statusText, const QString &note,
                                               const QString &subscriptionState, const QString &subscriptionReason,
                                               const QString & /*profileId*/)
{
    PresenceInfo info;
    info.entityUri     = entityUri;
    info.contactUri    = contactUri;
    info.basicStatus   = PresenceInfo::basicStatusFromString(basicStatus);
    info.note          = note.isEmpty() ? statusText : note;
    info.timestamp     = QDateTime::currentDateTimeUtc();
    info.expires       = AppSettings::presenceDefaultExpiresSeconds();
    info.subscriptionState  = PresenceInfo::subscriptionStateFromString(subscriptionState);
    info.subscriptionReason = subscriptionReason;
    info.contentType   = QStringLiteral("application/pidf+xml");
    info.parseStatus   = PresenceInfo::ParseStatus::Ok;

    if (activity.compare(QStringLiteral("away"), Qt::CaseInsensitive) == 0)
        info.extendedStatus = PresenceInfo::ExtendedStatus::Away;
    else if (activity.compare(QStringLiteral("busy"), Qt::CaseInsensitive) == 0)
        info.extendedStatus = PresenceInfo::ExtendedStatus::Busy;
    else if (info.basicStatus == PresenceInfo::BasicStatus::Open)
        info.extendedStatus = PresenceInfo::ExtendedStatus::Available;
    else if (info.basicStatus == PresenceInfo::BasicStatus::Closed)
        info.extendedStatus = PresenceInfo::ExtendedStatus::Offline;

    PresenceStore::instance().upsert(info);

    if (info.subscriptionState == PresenceInfo::SubscriptionState::Active) {
        // A live subscription is proof the peer accepted us — clear any
        // backoff state so a later termination starts counting from 1 again.
        m_presenceBackoffAttempts.remove(entityUri);
        return;
    }

    if (info.subscriptionState != PresenceInfo::SubscriptionState::Terminated)
        return;

    if (!AppSettings::enablePresence() || !AppSettings::enablePresenceSubscribe()
        || !AppSettings::presenceAutoResubscribe()) {
        return;
    }

    if (!PresenceResubscribePolicy::shouldAutoRetry(info.subscriptionReason)) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Presence auto-resubscribe skipped for %1: reason=%2 requires explicit action")
                .arg(entityUri, info.subscriptionReason));
        m_presenceBackoffAttempts.remove(entityUri);
        return;
    }

    schedulePresenceResubscribe(entityUri);
}

void SipManager::schedulePresenceResubscribe(const QString &entityUri)
{
    const int attempt = m_presenceBackoffAttempts.value(entityUri, 0) + 1;
    m_presenceBackoffAttempts[entityUri] = attempt;
    const int delayMs = PresenceResubscribePolicy::backoffMs(attempt);

    QTimer *timer = m_presenceBackoffTimers.value(entityUri);
    if (!timer) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        m_presenceBackoffTimers.insert(entityUri, timer);
        connect(timer, &QTimer::timeout, this, [this, entityUri]() {
            QString error;
            if (!subscribePresence(entityUri, error)) {
                Logger::instance().warn(LogCategory::Sip,
                    QStringLiteral("Presence auto-resubscribe failed for %1: %2").arg(entityUri, error));
            }
        });
    }

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Presence auto-resubscribe scheduled for %1: attempt=%2 delayMs=%3")
            .arg(entityUri).arg(attempt).arg(delayMs));
    timer->start(delayMs);
}

bool SipManager::answerCall()
{
    if (!m_activeCall || m_activeCall->state() != CallState::IncomingRinging) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("answerCall: no incoming call to answer"));
        return false;
    }
    // A previous audio-only outgoing call zeroes all video codec priorities;
    // restore them so an incoming video offer can actually be negotiated.
    applyVideoSettingsForCall();
    return m_activeCall->answer();
}

bool SipManager::rejectCall()
{
    if (!m_activeCall || m_activeCall->state() != CallState::IncomingRinging) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("rejectCall: no incoming call to reject"));
        return false;
    }
    return m_activeCall->reject();
}

bool SipManager::hangupCall()
{
    if (!m_activeCall || m_activeCall->state() == CallState::Idle
                      || m_activeCall->state() == CallState::Failed) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("hangupCall: no active call to hang up"));
        return false;
    }
    return m_activeCall->hangup();
}

bool SipManager::holdCall()
{
    if (!m_activeCall || m_activeCall->state() != CallState::Active) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("pauseCall: not in Active state"));
        return false;
    }
    return m_activeCall->hold();
}

bool SipManager::resumeCall()
{
    if (!m_activeCall || m_activeCall->state() != CallState::Held) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("resumeCall: not in Held state"));
        return false;
    }
    return m_activeCall->resume();
}

CallState SipManager::callState() const
{
    return m_activeCall ? m_activeCall->state() : CallState::Idle;
}

QString SipManager::callStatusText() const
{
    return m_activeCall ? m_activeCall->statusText()
                        : callStateDisplayText(CallState::Idle);
}

QString SipManager::activeCallRemoteUri() const
{
    return m_activeCall ? m_activeCall->remoteUri() : QString{};
}

QString SipManager::activeCallSipId() const
{
    return m_activeCall ? m_activeCall->callId() : QString{};
}

QString SipManager::activeCallDialogState() const
{
    return m_activeCall ? m_activeCall->sipDialogStateText() : QString{};
}

QString SipManager::activeCallRemoteMediaAddress() const
{
    return m_activeCall ? m_activeCall->remoteMediaAddress() : QString{};
}

QString SipManager::localTransportAddress() const
{
#ifdef HAVE_PJSIP
    if (m_ep) {
        for (int tid : std::as_const(m_ep->transportIds)) {
            try {
                pj::TransportInfo ti = m_ep->ep.transportGetInfo(tid);
                const QString addr = QString::fromStdString(ti.localName);
                if (!addr.isEmpty())
                    return addr;
            } catch (...) {}
        }
    }
#endif
    return {};
}

void SipManager::onAccountIncomingCall(const QString &remoteUri)
{
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Incoming call from %1").arg(remoteUri));

    if (m_activeCall && m_activeCall->state() != CallState::Idle
                     && m_activeCall->state() != CallState::Failed) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Incoming call rejected: a call is already active"));
        return;
    }

    destroyActiveCall();
    m_activeCall = new SipCall(this);
    wireActiveCall(m_activeCall);
    AudioMediaManager::instance().attachCall(m_activeCall);
    VideoMediaManager::instance().attachCall(m_activeCall);
    m_rttSession.enableForCall(m_activeCall);

    // Emit INVITE inbound trace.
    {
        SipMessageTrace trace;
        trace.direction = SipMessageTrace::Direction::Inbound;
        trace.method    = QStringLiteral("INVITE");
        trace.fromUri   = remoteUri;
        const SipProfile ip = SipProfileManager::instance().activeProfile();
        if (!ip.isNull())
            trace.toUri = ip.effectiveSipUri();
        SipTraceLogger::instance().logMessage(trace);
    }

    m_activeCall->stateMachine().tryTransition(CallState::IncomingRinging,
                                               QStringLiteral("Incoming call from %1")
                                                   .arg(remoteUri));
    emit incomingCall(remoteUri);
}

void SipManager::onActiveCallStateChanged(CallState state,
                                          const QString &statusText,
                                          int statusCode)
{
    emit callStateChanged(state, statusText, statusCode);

    // Emit SIP signaling traces for key call state transitions.
    const QString remUri = m_activeCall ? m_activeCall->remoteUri() : QString{};
    const QString cid    = m_activeCall ? m_activeCall->callId()    : QString{};
    const SipProfile cp  = SipProfileManager::instance().activeProfile();
    const QString localUri = cp.isNull() ? QString{} : cp.effectiveSipUri();

    if (state == CallState::Ringing) {
        // Outgoing: remote is ringing (180 Ringing inbound).
        SipMessageTrace trace;
        trace.direction  = SipMessageTrace::Direction::Inbound;
        trace.statusCode = 180;
        trace.statusText = QStringLiteral("Ringing");
        trace.method     = QStringLiteral("INVITE");
        trace.fromUri    = remUri;
        trace.toUri      = localUri;
        trace.callId     = cid;
        SipTraceLogger::instance().logMessage(trace);
    } else if (state == CallState::Active) {
        // 200 OK — direction depends on whether we originated the call.
        // In stub mode INVITE always originates from the makeCall() side,
        // so 200 OK is inbound. For incoming calls it would be outbound,
        // but the state machine doesn't expose call direction here; we use
        // Inbound as a reasonable stub-mode default.
        SipMessageTrace ok;
        ok.direction  = SipMessageTrace::Direction::Inbound;
        ok.statusCode = 200;
        ok.statusText = QStringLiteral("OK");
        ok.method     = QStringLiteral("INVITE");
        ok.fromUri    = remUri;
        ok.toUri      = localUri;
        ok.callId     = cid;
        SipTraceLogger::instance().logMessage(ok);

        // ACK is always outbound (sent by the UAS-answerer or UAC-original).
        SipMessageTrace ack;
        ack.direction = SipMessageTrace::Direction::Outbound;
        ack.method    = QStringLiteral("ACK");
        ack.fromUri   = localUri;
        ack.toUri     = remUri;
        ack.callId    = cid;
        SipTraceLogger::instance().logMessage(ack);
    } else if (state == CallState::Disconnecting) {
        // BYE outbound (we initiated the hang-up in stub mode).
        SipMessageTrace bye;
        bye.direction = SipMessageTrace::Direction::Outbound;
        bye.method    = QStringLiteral("BYE");
        bye.fromUri   = localUri;
        bye.toUri     = remUri;
        bye.callId    = cid;
        SipTraceLogger::instance().logMessage(bye);
    } else if (state == CallState::Failed && statusCode >= 400) {
        // Error response inbound.
        SipMessageTrace err;
        err.direction  = SipMessageTrace::Direction::Inbound;
        err.statusCode = statusCode;
        err.statusText = statusText;
        err.method     = QStringLiteral("INVITE");
        err.fromUri    = remUri;
        err.toUri      = localUri;
        err.callId     = cid;
        SipTraceLogger::instance().logMessage(err);
    }

    // Clean up the call object once it has fully ended.
    if (state == CallState::Idle || state == CallState::Failed) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Call ended in state %1; releasing call object")
                .arg(callStateName(state)));
        AudioMediaManager::instance().detachCall();
        VideoMediaManager::instance().detachCall();
        // Restore video codec priorities to the saved order (undoes any per-call disable).
        CodecManager::instance().applyVideoCodecOrder(
            VideoQualityManager::instance().current().codecOrder);
        // Defer Qt-object cleanup, but release the pj::Call slot now.
        // Without this, deleteLater fires after destroyAccount() when unregister
        // follows hangup immediately, producing "deleting account while call active".
        SipCall *call = m_activeCall;
        m_activeCall = nullptr;
        if (call) {
            call->releasePjsipCall();
            call->deleteLater();
        }
    }
}

void SipManager::setRefreshConfig(const RegistrationRefreshConfig &config)
{
    m_refreshConfig = config;
}

const RegistrationRefreshConfig &SipManager::refreshConfig() const
{
    return m_refreshConfig;
}

int SipManager::registrationExpirySeconds() const
{
    return m_registrationExpirySeconds;
}

#ifdef HAVE_PJSIP
bool SipManager::ensureTransport(SipTransport transport, int &transportId,
                                 QString &error)
{
    if (!m_ep) {
        error = QStringLiteral("PJSIP endpoint is not initialized");
        return false;
    }

    pjsip_transport_type_e type = PJSIP_TRANSPORT_UDP;
    switch (transport) {
    case SipTransport::UDP: type = PJSIP_TRANSPORT_UDP; break;
    case SipTransport::TCP: type = PJSIP_TRANSPORT_TCP; break;
    case SipTransport::TLS: type = PJSIP_TRANSPORT_TLS; break;
    }

    const int typeKey = static_cast<int>(type);
    if (m_ep->transportIds.contains(typeKey)) {
        transportId = m_ep->transportIds.value(typeKey);
        return true;
    }

    try {
        pj::TransportConfig config;
        config.port = 0;
        transportId = m_ep->ep.transportCreate(type, config);
        m_ep->transportIds.insert(typeKey, transportId);
        return true;
    } catch (const pj::Error &e) {
        error = QString::fromStdString(e.reason);
        return false;
    }
}
#endif

void SipManager::applyVideoSettingsForCall()
{
    const VideoSettings vs = VideoQualityManager::instance().current();

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video call settings: camera=\"%1\" "
                       "resolution=%2x%3 @ %4 fps  bitrate=%5 kbps  "
                       "codec=[%6]  overlay=%7")
            .arg(vs.cameraId.isEmpty() ? QStringLiteral("(default)") : vs.cameraId)
            .arg(vs.resolution.width()).arg(vs.resolution.height())
            .arg(vs.fps)
            .arg(vs.bitrateKbps)
            .arg(vs.codecOrder.isEmpty()
                     ? QStringLiteral("(none)")
                     : vs.codecOrder.join(QStringLiteral(", ")))
            .arg(vs.overlayEnabled ? QStringLiteral("on") : QStringLiteral("off")));

#ifdef HAVE_PJSIP
    if (!vs.codecOrder.isEmpty())
        CodecManager::instance().applyVideoCodecOrder(vs.codecOrder);

    if (!vs.codecOrder.isEmpty())
        CodecManager::instance().applyVideoCodecBitrate(vs.codecOrder.first(), vs.bitrateKbps);

    CodecManager::instance().applyVideoCodecFormat(vs.codecOrder, vs.resolution, vs.fps);
    if (m_account) {
        if (!m_account->applyVideoSettings()) {
            Logger::instance().warn(LogCategory::Media,
                QStringLiteral("Video settings could not be applied to PJSIP "
                               "(Qt preview still uses \"%1\")")
                    .arg(vs.cameraId.isEmpty() ? QStringLiteral("(default)") : vs.cameraId));
        }
    } else {
        Logger::instance().warn(LogCategory::Media,
            QStringLiteral("Video settings could not be applied to PJSIP: no active account"));
    }
#else
    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Video setting not applied to PJSIP: "
                       "stub backend active — settings are preferences only"));
#endif
}

void SipManager::applyPersistedAudioDevices()
{
#ifdef HAVE_PJSIP
    if (!m_ep || !m_initialized)
        return;

    MediaDeviceSelectionModel sel(&MediaDeviceManager::instance());
    const MediaDevice mic = sel.selectedMicrophone();
    const MediaDevice spk = sel.selectedSpeaker();

    const QString micName = mic.isNull() ? QString{} : mic.displayName;
    const QString spkName = spk.isNull() ? QString{} : spk.displayName;

    Logger::instance().info(LogCategory::Media,
        QStringLiteral("Applying audio device selection: mic=\"%1\" speaker=\"%2\"")
            .arg(micName.isEmpty() ? QStringLiteral("(default)") : micName,
                 spkName.isEmpty() ? QStringLiteral("(default)") : spkName));

    PjsipAudioMapper::applyDevicesByName(micName, spkName);
#endif
}
