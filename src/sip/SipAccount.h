#pragma once
#include <QObject>
#include <QString>

// SipAccount wraps a pjsua2 Account (future task).
// Reads configuration from SipProfileManager.
// Reads authentication credentials from CredentialStore at registration time.
//
// This header is a forward-declaration stub — implementation deferred to
// the SIP registration task. No PJSIP types appear here intentionally;
// pjsua2.hpp is included only in SipAccount.cpp when HAVE_PJSIP is defined.
class SipAccount : public QObject
{
    Q_OBJECT
public:
    explicit SipAccount(const QString &profileId, QObject *parent = nullptr);
    ~SipAccount() override;

    QString profileId() const;

    // Future: register(), unregister(), onRegState() callback
signals:
    void registrationStateChanged(const QString &profileId, const QString &state);

private:
    QString m_profileId;
};
