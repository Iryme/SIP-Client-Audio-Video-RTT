#pragma once
#ifdef _WIN32

#include "security/ICredentialBackend.h"

// Windows Credential Manager backend.
// Stores credentials using CredWriteW / CredReadW / CredDeleteW (Advapi32.lib).
// Target name format: SIPClient/sip/<profileId>/<username>
class WindowsCredentialBackend : public ICredentialBackend
{
public:
    QString backendName() const override;
    bool    isSecure()    const override;

    bool    store(const QString &key, const QString &username,
                  const QString &password, QString *errorOut = nullptr) override;
    QString load(const QString &key, const QString &username,
                 bool *found = nullptr, QString *errorOut = nullptr) override;
    bool    remove(const QString &key, const QString &username,
                   QString *errorOut = nullptr) override;
    bool    has(const QString &key, const QString &username) override;
};

#endif // _WIN32
