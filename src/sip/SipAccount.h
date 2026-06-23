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

signals:
    void registrationStateChanged(RegistrationState state,
                                  const QString &statusText,
                                  int statusCode);

private:
    struct Impl;

    void postRegistrationResult(RegistrationState state,
                                const QString &statusText,
                                int statusCode);

    QString m_profileId;
    Impl   *m_impl{nullptr};
};
