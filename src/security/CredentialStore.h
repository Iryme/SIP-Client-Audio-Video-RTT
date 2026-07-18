#pragma once
#include <QObject>
#include <QString>
#include <memory>

class ICredentialBackend;

// Secure credential store for SIP profile passwords.
// Production singleton uses the OS keychain backend (Windows Credential Manager
// or macOS Keychain Services).
// Tests inject a MemoryCredentialBackend via setBackend() before use.
//
// IMPORTANT: passwords are NEVER passed to Logger. Only profileId, username, and
// backend name are logged.
class CredentialStore : public QObject
{
    Q_OBJECT
public:
    static CredentialStore &instance();
    ~CredentialStore() override;

    // Replace the active backend (intended for unit tests only).
    // Must be called before the first credential operation.
    void setBackend(std::unique_ptr<ICredentialBackend> backend);

    QString backendName() const;
    bool    isSecureBackendAvailable() const;

    // Returns true on success. Logs metadata; never logs the password value.
    bool    storePassword(const QString &profileId, const QString &username,
                          const QString &password);

    // Returns the password on success.
    // Sets *found = false and returns {} when the credential is absent.
    QString loadPassword(const QString &profileId, const QString &username,
                         bool *found = nullptr);

    bool    deletePassword(const QString &profileId, const QString &username);
    bool    hasPassword(const QString &profileId, const QString &username);

signals:
    void credentialStored(const QString &profileId, const QString &username);
    void credentialLoaded(const QString &profileId, const QString &username);
    void credentialDeleted(const QString &profileId, const QString &username);
    void backendError(const QString &operation, const QString &error);

private:
    explicit CredentialStore(QObject *parent = nullptr);

    // Builds the unique storage key: "SIPClient/sip/<profileId>/<username>"
    static QString credentialKey(const QString &profileId, const QString &username);

    std::unique_ptr<ICredentialBackend> m_backend;
};
