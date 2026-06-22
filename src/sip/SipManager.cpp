#include "SipManager.h"
#include "core/Logger.h"

#ifdef HAVE_PJSIP
#include <pjsua2.hpp>
#endif

// ---------------------------------------------------------------------------
// PJSIP endpoint wrapper (defined only when PJSIP is compiled in)
// ---------------------------------------------------------------------------
#ifdef HAVE_PJSIP

struct SipManager::PjEndpoint {
    pj::Endpoint ep;
};

static bool initPjsip(SipManager::PjEndpoint *&out, QString &errOut)
{
    out = new SipManager::PjEndpoint;
    try {
        out->ep.libCreate();

        pj::EpConfig cfg;
        cfg.logConfig.level      = 3;
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
    if (!ep) return;
    try {
        ep->ep.libDestroy();
    } catch (...) {}
    delete ep;
    ep = nullptr;
}

#endif // HAVE_PJSIP

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SipManager &SipManager::instance()
{
    static SipManager s_instance;
    return s_instance;
}

SipManager::SipManager() : QObject(nullptr) {}

SipManager::~SipManager()
{
    if (m_initialized)
        shutdown();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
bool SipManager::initialize()
{
    if (m_initialized)
        return true;

    Logger::instance().info(LogCategory::Sip, "Initializing SIP backend");

#ifdef HAVE_PJSIP
    if (!initPjsip(m_ep, m_lastError)) {
        Logger::instance().error(LogCategory::Sip,
            QStringLiteral("SIP backend initialization failed: %1").arg(m_lastError));
        emit initializationFailed(m_lastError);
        return false;
    }
    m_initialized = true;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("SIP backend initialized (%1)").arg(backendName()));
#else
    Logger::instance().warn(LogCategory::Sip,
        "PJSIP unavailable — running stub SIP backend");
    m_initialized = true;
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("SIP backend initialized (%1)").arg(backendName()));
#endif

    emit initialized();
    return true;
}

void SipManager::shutdown()
{
    if (!m_initialized)
        return;

    Logger::instance().info(LogCategory::Sip, "Shutting down SIP backend");

#ifdef HAVE_PJSIP
    shutdownPjsip(m_ep);
#endif

    m_initialized = false;
    Logger::instance().info(LogCategory::Sip, "SIP backend shut down");
    emit shutdownComplete();
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------
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
