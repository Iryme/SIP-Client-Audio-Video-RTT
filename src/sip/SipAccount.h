#pragma once

#include <QObject>
#include <QString>
#include <QMetaType>

#include "sip/SipProfile.h"

enum class RegistrationState {
    Unregistered,
    Registering,
    Registered,
    Unregistering,
    RegistrationFailed
};

Q_DECLARE_METATYPE(RegistrationState)

QString registrationStateName(RegistrationState state);

// Wraps one pjsua2 Account when PJSIP is available. PJSIP details are hidden
// behind Impl so stub builds never include pjsua2 headers.
class SipAccount : public QObject
{
    Q_OBJECT
public:
    explicit SipAccount(const QString &profileId, QObject *parent = nullptr);
    ~SipAccount() override;

    QString profileId() const;

    bool startRegistration(const SipProfile &profile, const QString &password,
                           int transportId = -1);
    bool startUnregistration();
    bool refreshRegistration();

    // Returns the internal pj::Account as a void* for use by SipCall in PJSIP mode.
    // Returns nullptr in stub mode. Cast to pj::Account* inside HAVE_PJSIP guards.
    void *pjAccountHandle() const;

signals:
    void registrationStateChanged(RegistrationState state,
                                  const QString &statusText,
                                  int statusCode);
    void registrationExpiryReceived(int seconds);

    // Emitted when a new incoming call arrives (PJSIP onIncomingCall callback).
    // In stub mode this signal is never emitted.
    void incomingCallReceived(const QString &remoteUri);

private:
    struct Impl;

    void postRegistrationResult(RegistrationState state,
                                const QString &statusText,
                                int statusCode,
                                int expirySeconds = 0);

    QString m_profileId;
    Impl   *m_impl{nullptr};
};
