#include "security/MemoryCredentialBackend.h"

QString MemoryCredentialBackend::backendName() const
{
    return QStringLiteral("Memory (test-only, NOT secure)");
}

bool MemoryCredentialBackend::isSecure() const
{
    return false;
}

bool MemoryCredentialBackend::store(const QString &key, const QString &username,
                                     const QString &password, QString *errorOut)
{
    Q_UNUSED(errorOut)
    m_data.insert(entryKey(key, username), password);
    return true;
}

QString MemoryCredentialBackend::load(const QString &key, const QString &username,
                                       bool *found, QString *errorOut)
{
    Q_UNUSED(errorOut)
    const auto it = m_data.constFind(entryKey(key, username));
    if (it == m_data.constEnd()) {
        if (found) *found = false;
        return {};
    }
    if (found) *found = true;
    return it.value();
}

bool MemoryCredentialBackend::remove(const QString &key, const QString &username,
                                      QString *errorOut)
{
    Q_UNUSED(errorOut)
    return m_data.remove(entryKey(key, username)) != 0;
}

bool MemoryCredentialBackend::has(const QString &key, const QString &username)
{
    return m_data.contains(entryKey(key, username));
}
