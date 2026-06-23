#pragma once

#include <QObject>
#include <QString>

#include "sip/SipAccount.h"
#include "sip/RegistrationStateMachine.h"

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

    // Exposes the state machine for testing (timeout injection, spy connections).
    RegistrationStateMachine &stateMachine();

signals:
    void initialized();
    void shutdownComplete();
    void initializationFailed(const QString &reason);
    void registrationStateChanged(RegistrationState state,
                                  const QString     &statusText,
                                  int                statusCode);

private slots:
    void onAccountRegistrationStateChanged(RegistrationState state,
                                           const QString     &statusText,
                                           int                statusCode);
    void onStateMachineTimedOut(RegistrationState stuckState);

private:
    SipManager();
    ~SipManager() override;

    void destroyAccount();

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
};
