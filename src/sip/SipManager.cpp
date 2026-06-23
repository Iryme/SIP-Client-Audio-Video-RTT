#include "SipManager.h"

#include <QPointer>

#include "core/Logger.h"
#include "media/AudioMediaManager.h"
#include "security/CredentialStore.h"
#include "sip/SipProfileManager.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/RegistrationRefreshConfig.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#include <QHash>
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
        ep->ep.libDestroy();
    } catch (...) {
    }
    delete ep;
    ep = nullptr;
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
    if (!initPjsip(m_ep, m_lastError)) {
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("SIP backend initialization failed: %1").arg(m_lastError));
        emit initializationFailed(m_lastError);
        return false;
    }
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

    m_registeredProfileId = profile.profileId;
    m_account = new SipAccount(profile.profileId, this);
    connect(m_account, &SipAccount::registrationStateChanged,
            this, &SipManager::onAccountRegistrationStateChanged);
    connect(m_account, &SipAccount::registrationExpiryReceived,
            this, &SipManager::onAccountRegistrationExpiryReceived);
    connect(m_account, &SipAccount::incomingCallReceived,
            this, &SipManager::onAccountIncomingCall);

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
        m_retryAttempt = 0;
        scheduleRefresh(m_registrationExpirySeconds);
    } else if (state == RegistrationState::RegistrationFailed) {
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("Register failed for profile %1: %2 (status %3)")
                .arg(m_registeredProfileId, statusText).arg(statusCode));
        scheduleRetryIfEligible(statusCode);
    } else if (state == RegistrationState::Unregistered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Unregister success for profile %1 (status %2)")
                .arg(m_registeredProfileId).arg(statusCode));
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

void SipManager::destroyActiveCall()
{
    if (!m_activeCall)
        return;
    AudioMediaManager::instance().detachCall();
    disconnect(m_activeCall, nullptr, this, nullptr);
    delete m_activeCall;
    m_activeCall = nullptr;
}

bool SipManager::makeCall(const QString &remoteUri)
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
    connect(m_activeCall, &SipCall::callStateChanged,
            this, &SipManager::onActiveCallStateChanged);
    connect(m_activeCall, &SipCall::callConnected,
            this, &SipManager::callConnected);
    connect(m_activeCall, &SipCall::callDisconnected,
            this, &SipManager::callDisconnected);
    connect(m_activeCall, &SipCall::callFailed,
            this, &SipManager::callFailed);
    connect(m_activeCall, &SipCall::audioMediaConnected,
            this, &SipManager::audioMediaConnected);
    connect(m_activeCall, &SipCall::audioMediaDisconnected,
            this, &SipManager::audioMediaDisconnected);
    connect(m_activeCall, &SipCall::muteChanged,
            this, &SipManager::callMuteChanged);
    connect(m_activeCall, &SipCall::inputLevelChanged,
            this, &SipManager::callInputLevelChanged);
    connect(m_activeCall, &SipCall::outputLevelChanged,
            this, &SipManager::callOutputLevelChanged);
    AudioMediaManager::instance().attachCall(m_activeCall);

    return m_activeCall->makeCall(remoteUri);
}

bool SipManager::answerCall()
{
    if (!m_activeCall || m_activeCall->state() != CallState::IncomingRinging) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("answerCall: no incoming call to answer"));
        return false;
    }
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
            QStringLiteral("holdCall: not in Active state"));
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
    connect(m_activeCall, &SipCall::callStateChanged,
            this, &SipManager::onActiveCallStateChanged);
    connect(m_activeCall, &SipCall::callConnected,
            this, &SipManager::callConnected);
    connect(m_activeCall, &SipCall::callDisconnected,
            this, &SipManager::callDisconnected);
    connect(m_activeCall, &SipCall::callFailed,
            this, &SipManager::callFailed);
    connect(m_activeCall, &SipCall::audioMediaConnected,
            this, &SipManager::audioMediaConnected);
    connect(m_activeCall, &SipCall::audioMediaDisconnected,
            this, &SipManager::audioMediaDisconnected);
    connect(m_activeCall, &SipCall::muteChanged,
            this, &SipManager::callMuteChanged);
    connect(m_activeCall, &SipCall::inputLevelChanged,
            this, &SipManager::callInputLevelChanged);
    connect(m_activeCall, &SipCall::outputLevelChanged,
            this, &SipManager::callOutputLevelChanged);
    AudioMediaManager::instance().attachCall(m_activeCall);
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

    // Clean up the call object once it has fully ended.
    if (state == CallState::Idle || state == CallState::Failed) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Call ended in state %1; releasing call object")
                .arg(callStateName(state)));
        AudioMediaManager::instance().detachCall();
        // Defer destruction so signal handlers in the call finish first.
        SipCall *call = m_activeCall;
        m_activeCall = nullptr;
        if (call)
            call->deleteLater();
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
