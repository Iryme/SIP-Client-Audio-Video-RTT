#pragma once
#include <QString>

// Pure interface for credential storage backends.
// Implementations: WindowsCredentialBackend and MacKeychainBackend (production),
// MemoryCredentialBackend (tests).
class ICredentialBackend
{
public:
    virtual ~ICredentialBackend() = default;

    // Human-readable name of this backend (logged on init, never contains secrets).
    virtual QString backendName() const = 0;

    // Returns true if this backend provides OS-level encryption.
    virtual bool isSecure() const = 0;

    // Store a password for (key, username). Returns false and sets errorOut on failure.
    virtual bool store(const QString &key, const QString &username,
                       const QString &password, QString *errorOut = nullptr) = 0;

    // Load a password for (key, username).
    // Sets *found=false and returns {} when the credential does not exist.
    virtual QString load(const QString &key, const QString &username,
                         bool *found = nullptr, QString *errorOut = nullptr) = 0;

    // Delete a credential. Returns false if not found or on error; sets errorOut on error.
    virtual bool remove(const QString &key, const QString &username,
                        QString *errorOut = nullptr) = 0;

    // Returns true if a credential exists for (key, username).
    virtual bool has(const QString &key, const QString &username) = 0;
};
