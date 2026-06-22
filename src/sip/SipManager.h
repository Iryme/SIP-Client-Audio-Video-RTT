#pragma once

#include <QObject>
#include <QString>

#include "sip/SipAccount.h"

// Owns the SIP endpoint lifecycle and the account for the active profile.
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

    bool              isInitialized() const;
    bool              isPjsipAvailable() const;
    QString           backendName() const;
    QString           lastError() const;
    RegistrationState registrationState() const;
    QString           registrationStatusText() const;
    int               registrationStatusCode() const;
    QString           registeredProfileId() const;

signals:
    void initialized();
    void shutdownComplete();
    void initializationFailed(const QString &reason);
    void registrationStateChanged(RegistrationState state,
                                  const QString &statusText,
                                  int statusCode);

private slots:
    void onAccountRegistrationStateChanged(RegistrationState state,
                                           const QString &statusText,
                                           int statusCode);

private:
    SipManager();
    ~SipManager() override;

    void setRegistrationState(RegistrationState state,
                              const QString &statusText,
                              int statusCode = 0);
    void destroyAccount();

#ifdef HAVE_PJSIP
    bool ensureTransport(SipTransport transport, int &transportId, QString &error);

    struct PjEndpoint;
    PjEndpoint *m_ep{nullptr};
#endif

    bool              m_initialized{false};
    QString           m_lastError;
    SipAccount       *m_account{nullptr};
    RegistrationState m_registrationState{RegistrationState::Unregistered};
    QString           m_registrationStatusText{QStringLiteral("Unregistered")};
    int               m_registrationStatusCode{0};
    QString           m_registeredProfileId;
};
