#pragma once

#ifdef __APPLE__

#include "security/ICredentialBackend.h"

// macOS Keychain Services backend. Password bytes are stored as UTF-8 generic
// password data and are never persisted through QSettings or written to logs.
class MacKeychainBackend final : public ICredentialBackend
{
public:
    QString backendName() const override;
    bool isSecure() const override;
    bool store(const QString &key, const QString &username,
               const QString &password, QString *errorOut = nullptr) override;
    QString load(const QString &key, const QString &username,
                 bool *found = nullptr, QString *errorOut = nullptr) override;
    bool remove(const QString &key, const QString &username,
                QString *errorOut = nullptr) override;
    bool has(const QString &key, const QString &username) override;
};

#endif
