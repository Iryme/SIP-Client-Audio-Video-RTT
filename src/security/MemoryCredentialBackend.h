#pragma once
#include "security/ICredentialBackend.h"
#include <QHash>
#include <QString>

// In-memory credential backend for unit tests.
// NOT secure — data is held in plain text in process memory.
// NEVER use this backend in production code.
class MemoryCredentialBackend : public ICredentialBackend
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

    // Test helper: number of entries currently stored.
    int count() const { return m_data.size(); }
    // Test helper: clear all stored credentials.
    void clear() { m_data.clear(); }

private:
    static QString entryKey(const QString &key, const QString &username)
    { return key + QChar('\x1F') + username; }

    QHash<QString, QString> m_data;
};
