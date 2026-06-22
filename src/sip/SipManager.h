#pragma once
#include <QObject>
#include <QString>

// SipManager owns the SIP endpoint lifecycle.
// When HAVE_PJSIP is defined (ENABLE_PJSIP=ON and PJSIP found):
//   initialize() creates and starts the pjsua2 Endpoint.
//   shutdown() cleanly destroys it.
// Without HAVE_PJSIP:
//   All methods succeed without side effects (stub mode).
//   backendName() returns "Stub SIP backend".
//
// Accounts and calls are managed by SipAccount / SipCall (future tasks).
// This class does NOT register any account.
class SipManager : public QObject
{
    Q_OBJECT
public:
    static SipManager &instance();

    // Lifecycle — safe to call from the Qt main thread.
    bool initialize();
    void shutdown();

    // State queries
    bool    isInitialized()     const;
    bool    isPjsipAvailable()  const;
    QString backendName()       const;
    QString lastError()         const;

signals:
    void initialized();
    void shutdownComplete();
    void initializationFailed(const QString &reason);

private:
    SipManager();
    ~SipManager() override;

    bool    m_initialized{false};
    QString m_lastError;

#ifdef HAVE_PJSIP
    // pjsua2 Endpoint is owned here when PJSIP is compiled in.
    // Forward-declared to keep pjsua2.hpp out of every translation unit.
    struct PjEndpoint;
    PjEndpoint *m_ep{nullptr};
#endif
};
