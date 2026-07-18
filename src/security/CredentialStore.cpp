#include "security/CredentialStore.h"
#include "security/ICredentialBackend.h"
#include "core/Logger.h"

#ifdef _WIN32
#  include "security/WindowsCredentialBackend.h"
#elif defined(__APPLE__)
#  include "security/MacKeychainBackend.h"
#endif

// ---- singleton -------------------------------------------------------

CredentialStore &CredentialStore::instance()
{
    static CredentialStore s_instance;
    return s_instance;
}

CredentialStore::CredentialStore(QObject *parent)
    : QObject(parent)
{
#ifdef _WIN32
    m_backend = std::make_unique<WindowsCredentialBackend>();
    Logger::instance().info(LogCategory::Platform,
        QStringLiteral("CredentialStore: using backend '%1'").arg(m_backend->backendName()));
#elif defined(__APPLE__)
    m_backend = std::make_unique<MacKeychainBackend>();
    Logger::instance().info(LogCategory::Platform,
        QStringLiteral("CredentialStore: using backend '%1'").arg(m_backend->backendName()));
#else
    Logger::instance().warn(LogCategory::Platform,
        QStringLiteral("CredentialStore: no secure backend available on this platform — "
                       "credential operations will fail"));
#endif
}

CredentialStore::~CredentialStore() = default;

// ---- backend injection -----------------------------------------------

void CredentialStore::setBackend(std::unique_ptr<ICredentialBackend> backend)
{
    if (backend) {
        Logger::instance().info(LogCategory::Platform,
            QStringLiteral("CredentialStore: backend replaced with '%1'")
                .arg(backend->backendName()));
    } else {
        Logger::instance().warn(LogCategory::Platform,
            QStringLiteral("CredentialStore: backend set to null — credential operations will fail"));
    }
    m_backend = std::move(backend);
}

// ---- public query ----------------------------------------------------

QString CredentialStore::backendName() const
{
    return m_backend ? m_backend->backendName() : QStringLiteral("none");
}

bool CredentialStore::isSecureBackendAvailable() const
{
    return m_backend && m_backend->isSecure();
}

// ---- key helper ------------------------------------------------------

QString CredentialStore::credentialKey(const QString &profileId, const QString &username)
{
    return QStringLiteral("SIPClient/sip/") + profileId + QChar('/') + username;
}

// ---- credential operations ------------------------------------------

bool CredentialStore::storePassword(const QString &profileId, const QString &username,
                                    const QString &password)
{
    if (!m_backend) {
        const QString err = QStringLiteral("no backend available");
        Logger::instance().error(LogCategory::Platform,
            QStringLiteral("CredentialStore: store failed for profile '%1' user '%2': %3")
                .arg(profileId, username, err));
        emit backendError(QStringLiteral("store"), err);
        return false;
    }

    QString error;
    const bool ok = m_backend->store(credentialKey(profileId, username), username, password, &error);
    if (ok) {
        Logger::instance().info(LogCategory::Platform,
            QStringLiteral("CredentialStore: credential stored — profile '%1' user '%2' backend '%3'")
                .arg(profileId, username, m_backend->backendName()));
        emit credentialStored(profileId, username);
    } else {
        Logger::instance().error(LogCategory::Platform,
            QStringLiteral("CredentialStore: store failed — profile '%1' user '%2': %3")
                .arg(profileId, username, error));
        emit backendError(QStringLiteral("store"), error);
    }
    return ok;
}

QString CredentialStore::loadPassword(const QString &profileId, const QString &username,
                                      bool *found)
{
    if (found) *found = false;

    if (!m_backend) {
        const QString err = QStringLiteral("no backend available");
        Logger::instance().error(LogCategory::Platform,
            QStringLiteral("CredentialStore: load failed for profile '%1' user '%2': %3")
                .arg(profileId, username, err));
        emit backendError(QStringLiteral("load"), err);
        return {};
    }

    bool localFound = false;
    QString error;
    const QString password = m_backend->load(credentialKey(profileId, username), username,
                                              &localFound, &error);
    if (found) *found = localFound;

    if (localFound) {
        Logger::instance().info(LogCategory::Platform,
            QStringLiteral("CredentialStore: credential loaded — profile '%1' user '%2' backend '%3'")
                .arg(profileId, username, m_backend->backendName()));
        emit credentialLoaded(profileId, username);
    } else if (!error.isEmpty()) {
        Logger::instance().error(LogCategory::Platform,
            QStringLiteral("CredentialStore: load error — profile '%1' user '%2': %3")
                .arg(profileId, username, error));
        emit backendError(QStringLiteral("load"), error);
    } else {
        Logger::instance().debug(LogCategory::Platform,
            QStringLiteral("CredentialStore: credential not found — profile '%1' user '%2'")
                .arg(profileId, username));
    }
    return password;
}

bool CredentialStore::deletePassword(const QString &profileId, const QString &username)
{
    if (!m_backend) {
        const QString err = QStringLiteral("no backend available");
        Logger::instance().error(LogCategory::Platform,
            QStringLiteral("CredentialStore: delete failed for profile '%1' user '%2': %3")
                .arg(profileId, username, err));
        emit backendError(QStringLiteral("delete"), err);
        return false;
    }

    QString error;
    const bool ok = m_backend->remove(credentialKey(profileId, username), username, &error);
    if (ok) {
        Logger::instance().info(LogCategory::Platform,
            QStringLiteral("CredentialStore: credential deleted — profile '%1' user '%2' backend '%3'")
                .arg(profileId, username, m_backend->backendName()));
        emit credentialDeleted(profileId, username);
    } else if (!error.isEmpty()) {
        Logger::instance().error(LogCategory::Platform,
            QStringLiteral("CredentialStore: delete error — profile '%1' user '%2': %3")
                .arg(profileId, username, error));
        emit backendError(QStringLiteral("delete"), error);
    } else {
        Logger::instance().debug(LogCategory::Platform,
            QStringLiteral("CredentialStore: delete — credential not found for profile '%1' user '%2'")
                .arg(profileId, username));
    }
    return ok;
}

bool CredentialStore::hasPassword(const QString &profileId, const QString &username)
{
    if (!m_backend)
        return false;
    return m_backend->has(credentialKey(profileId, username), username);
}
