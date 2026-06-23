#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "sip/SipAccount.h"
#include "sip/RegistrationStateMachine.h"
#include "sip/RegistrationRetryPolicy.h"

// Owns the SIP endpoint lifecycle and the account for the active profile.
// Registration flow is governed by RegistrationStateMachine; invalid operations
// (e.g. register while Registering) are rejected without side effects.
// Credentials are fetched from CredentialStore only for a registration attempt
// and are never retained by SipManager, logged, or persisted.
class SipManager : public QObject
{
    Q_OBJECT
public:
    static SipManager &instance();

    bool initialize();
    void shutdown();

    bool registerActiveProfile();
    bool unregisterActiveProfile();

    bool    isInitialized()        const;
    bool    isPjsipAvailable()     const;
    QString backendName()          const;
    QString lastError()            const;

    RegistrationState registrationState()       const;
    QString           registrationStatusText()  const;
    int               registrationStatusCode()  const;
    QString           registeredProfileId()     const;

    void                         setRetryPolicy(const RegistrationRetryPolicy &policy);
    const RegistrationRetryPolicy &retryPolicy() const;
    int                          retryAttempt() const;

    // Exposes the state machine for testing (timeout injection, spy connections).
    RegistrationStateMachine &stateMachine();

signals:
    void initialized();
    void shutdownComplete();
    void initializationFailed(const QString &reason);
    void registrationStateChanged(RegistrationState state,
                                  const QString     &statusText,
                                  int                statusCode);
    void retryScheduled(int attempt, int delayMs);

private slots:
    void onAccountRegistrationStateChanged(RegistrationState state,
                                           const QString     &statusText,
                                           int                statusCode);
    void onStateMachineTimedOut(RegistrationState stuckState);
    void onRetryTimerFired();

private:
    SipManager();
    ~SipManager() override;

    void destroyAccount();
    void scheduleRetryIfEligible(int statusCode);

#ifdef HAVE_PJSIP
    bool ensureTransport(SipTransport transport, int &transportId, QString &error);

    struct PjEndpoint;
    PjEndpoint *m_ep{nullptr};
#endif

    bool                     m_initialized{false};
    QString                  m_lastError;
    SipAccount              *m_account{nullptr};
    QString                  m_registeredProfileId;
    RegistrationStateMachine m_stateMachine;
    RegistrationRetryPolicy  m_retryPolicy;
    QTimer                   m_retryTimer;
    int                      m_retryAttempt{0};
};
