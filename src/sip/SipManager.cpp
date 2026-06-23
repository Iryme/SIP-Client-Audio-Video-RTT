#include "SipManager.h"

#include "core/Logger.h"
#include "security/CredentialStore.h"
#include "sip/SipProfileManager.h"

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

    connect(&m_stateMachine, &RegistrationStateMachine::stateChanged,
            this, &SipManager::registrationStateChanged);
    connect(&m_stateMachine, &RegistrationStateMachine::transitionTimedOut,
            this, &SipManager::onStateMachineTimedOut);
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

void SipManager::onAccountRegistrationStateChanged(RegistrationState state,
                                                   const QString &statusText,
                                                   int statusCode)
{
    m_stateMachine.tryTransition(state, statusText, statusCode);

    if (state == RegistrationState::Registered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Register success for profile %1 (status %2)")
                .arg(m_registeredProfileId).arg(statusCode));
    } else if (state == RegistrationState::RegistrationFailed) {
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("Register failed for profile %1: %2 (status %3)")
                .arg(m_registeredProfileId, statusText).arg(statusCode));
    } else if (state == RegistrationState::Unregistered) {
        Logger::instance().info(LogCategory::Sip,
            QStringLiteral("Unregister success for profile %1 (status %2)")
                .arg(m_registeredProfileId).arg(statusCode));
        destroyAccount();
    }
}

void SipManager::onStateMachineTimedOut(RegistrationState stuckState)
{
    Logger::instance().warn(LogCategory::Sip,
        QStringLiteral("Registration state machine timed out in %1; cleaning up account")
            .arg(registrationStateName(stuckState)));
    destroyAccount();
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
