#include "sip/SipProfileManager.h"
#include "core/Logger.h"
#include "security/CredentialStore.h"
#include <QSet>

// -----------------------------------------------------------------------
// Singleton
// -----------------------------------------------------------------------
namespace {
QString &sipProfileManagerConfigDirOverride()
{
    static QString s_dir;
    return s_dir;
}
}

void SipProfileManager::setConfigDirectoryOverride(const QString &dir)
{
    sipProfileManagerConfigDirOverride() = dir;
}

SipProfileManager &SipProfileManager::instance()
{
    const QString &dir = sipProfileManagerConfigDirOverride();
    if (!dir.isEmpty()) {
        static SipProfileManager s_instanceOverride(
            dir + QStringLiteral("/SIPClientProfiles.ini"), QSettings::IniFormat, nullptr);
        return s_instanceOverride;
    }
    static SipProfileManager s_instance("SIPClient", "SIPClientProfiles");
    return s_instance;
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
SipProfileManager::SipProfileManager(const QString &org, const QString &app,
                                      QSettings::Format format, QObject *parent)
    : QObject(parent)
    , m_settings(format, QSettings::UserScope, org, app)
{
    loadAll();
}

SipProfileManager::SipProfileManager(const QString &iniFilePath, QSettings::Format format,
                                      QObject *parent)
    : QObject(parent)
    , m_settings(iniFilePath, format)
{
    loadAll();
}

// -----------------------------------------------------------------------
// Validation
// -----------------------------------------------------------------------
ProfileValidationResult SipProfileManager::validate(const SipProfile &profile) const
{
    ProfileValidationResult result;
    result.valid = true;

    if (profile.displayName.trimmed().isEmpty()) {
        result.errors << QStringLiteral("Display name is required");
        result.valid = false;
    }
    if (profile.sipUsername.trimmed().isEmpty()) {
        result.errors << QStringLiteral("SIP username is required");
        result.valid = false;
    }
    if (profile.sipDomain.trimmed().isEmpty()) {
        result.errors << QStringLiteral("SIP domain is required");
        result.valid = false;
    }
    if (profile.registrar.trimmed().isEmpty()) {
        result.errors << QStringLiteral("Registrar address is required");
        result.valid = false;
    }
    // Transport is always valid as an enum value

    return result;
}

// Returns authUsername if set, otherwise sipUsername (used as CredentialStore key).
static QString effectiveAuthUsername(const SipProfile &p)
{
    return p.authUsername.isEmpty() ? p.sipUsername : p.authUsername;
}

// -----------------------------------------------------------------------
// CRUD
// -----------------------------------------------------------------------
QString SipProfileManager::add(SipProfile profile)
{
    const auto result = validate(profile);
    if (!result.valid) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Profile validation failed: ") + result.errors.join("; "));
        return {};
    }

    if (profile.profileId.isEmpty())
        profile = SipProfile::createNew();
    else if (profile.createdAt.isNull())
        profile.createdAt = QDateTime::currentDateTimeUtc();

    // Derive sipUri if not set
    if (profile.sipUri.isEmpty())
        profile.sipUri = profile.effectiveSipUri();

    profile.updatedAt = QDateTime::currentDateTimeUtc();

    m_profiles.append(profile);
    saveProfile(profile);
    m_settings.sync();

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile added: %1 (%2)").arg(profile.displayName, profile.profileId));
    emit profileAdded(profile.profileId);
    return profile.profileId;
}

bool SipProfileManager::update(const SipProfile &profile)
{
    const int idx = [&]() {
        for (int i = 0; i < m_profiles.size(); ++i)
            if (m_profiles[i].profileId == profile.profileId) return i;
        return -1;
    }();

    if (idx < 0) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Update failed — profile not found: %1").arg(profile.profileId));
        return false;
    }

    const auto result = validate(profile);
    if (!result.valid) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Profile update validation failed: ") + result.errors.join("; "));
        return false;
    }

    SipProfile updated = profile;
    if (updated.sipUri.isEmpty())
        updated.sipUri = updated.effectiveSipUri();
    updated.updatedAt = QDateTime::currentDateTimeUtc();

    m_profiles[idx] = updated;
    saveProfile(updated);
    m_settings.sync();

    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile updated: %1 (%2)").arg(updated.displayName, updated.profileId));
    emit profileUpdated(updated.profileId);
    return true;
}

bool SipProfileManager::remove(const QString &profileId)
{
    const int idx = [&]() {
        for (int i = 0; i < m_profiles.size(); ++i)
            if (m_profiles[i].profileId == profileId) return i;
        return -1;
    }();

    if (idx < 0) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Remove failed — profile not found: %1").arg(profileId));
        return false;
    }

    const QString displayName = m_profiles[idx].displayName;
    const SipProfile removedProfile = m_profiles[idx];
    m_profiles.removeAt(idx);
    removeFromStorage(profileId);

    // Delete any stored credential; graceful if none exists
    CredentialStore::instance().deletePassword(profileId, effectiveAuthUsername(removedProfile));

    if (m_activeProfileId == profileId) {
        m_activeProfileId.clear();
        m_settings.remove("active");
        m_settings.sync();
        emit activeProfileChanged(QString{});
    }

    m_settings.sync();
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Profile deleted: %1 (%2)").arg(displayName, profileId));
    emit profileRemoved(profileId);
    return true;
}

// -----------------------------------------------------------------------
// Queries
// -----------------------------------------------------------------------
QList<SipProfile> SipProfileManager::profiles() const
{
    return m_profiles;
}

SipProfile SipProfileManager::profile(const QString &profileId) const
{
    for (const SipProfile &p : m_profiles)
        if (p.profileId == profileId) return p;
    return {};
}

bool SipProfileManager::hasProfile(const QString &profileId) const
{
    for (const SipProfile &p : m_profiles)
        if (p.profileId == profileId) return true;
    return false;
}

// -----------------------------------------------------------------------
// Active profile
// -----------------------------------------------------------------------
void SipProfileManager::setActiveProfileId(const QString &id)
{
    if (id == m_activeProfileId) return;

    if (!id.isEmpty() && !hasProfile(id)) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("setActiveProfileId: unknown profile id %1").arg(id));
        return;
    }

    m_activeProfileId = id;
    if (id.isEmpty())
        m_settings.remove("active");
    else
        m_settings.setValue("active/profileId", id);
    m_settings.sync();

    Logger::instance().info(LogCategory::Sip,
        id.isEmpty()
            ? QStringLiteral("Active profile cleared")
            : QStringLiteral("Active profile selected: %1").arg(id));
    emit activeProfileChanged(id);
}

QString SipProfileManager::activeProfileId() const
{
    return m_activeProfileId;
}

bool SipProfileManager::hasActiveProfile() const
{
    return !m_activeProfileId.isEmpty() && hasProfile(m_activeProfileId);
}

SipProfile SipProfileManager::activeProfile() const
{
    return profile(m_activeProfileId);
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------
void SipProfileManager::sync()
{
    m_settings.sync();
}

// -----------------------------------------------------------------------
// Credential helpers
// -----------------------------------------------------------------------

bool SipProfileManager::setProfilePassword(const QString &profileId, const QString &password)
{
    const SipProfile p = profile(profileId);
    if (p.isNull()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("setProfilePassword: profile not found: %1").arg(profileId));
        return false;
    }
    return CredentialStore::instance().storePassword(profileId, effectiveAuthUsername(p), password);
}

bool SipProfileManager::hasProfilePassword(const QString &profileId) const
{
    const SipProfile p = profile(profileId);
    if (p.isNull()) return false;
    return CredentialStore::instance().hasPassword(profileId, effectiveAuthUsername(p));
}

bool SipProfileManager::removeProfilePassword(const QString &profileId)
{
    const SipProfile p = profile(profileId);
    if (p.isNull()) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("removeProfilePassword: profile not found: %1").arg(profileId));
        return false;
    }
    return CredentialStore::instance().deletePassword(profileId, effectiveAuthUsername(p));
}

// -----------------------------------------------------------------------
// Transport helpers
// -----------------------------------------------------------------------
QString SipProfileManager::transportToString(SipTransport t)
{
    switch (t) {
    case SipTransport::TCP: return QStringLiteral("TCP");
    case SipTransport::TLS: return QStringLiteral("TLS");
    case SipTransport::UDP: return QStringLiteral("UDP");
    }
    return QStringLiteral("UDP");
}

SipTransport SipProfileManager::transportFromString(const QString &s, bool *ok)
{
    if (s.compare(QStringLiteral("TCP"), Qt::CaseInsensitive) == 0) {
        if (ok) *ok = true;
        return SipTransport::TCP;
    }
    if (s.compare(QStringLiteral("TLS"), Qt::CaseInsensitive) == 0) {
        if (ok) *ok = true;
        return SipTransport::TLS;
    }
    if (s.compare(QStringLiteral("UDP"), Qt::CaseInsensitive) == 0) {
        if (ok) *ok = true;
        return SipTransport::UDP;
    }
    if (ok) *ok = false;
    return SipTransport::UDP; // safe fallback
}

// -----------------------------------------------------------------------
// Private persistence helpers
// -----------------------------------------------------------------------
void SipProfileManager::loadAll()
{
    m_profiles.clear();

    QSet<QString> idSet;
    const QStringList keys = m_settings.allKeys();
    for (const QString &key : keys) {
        if (!key.startsWith(QStringLiteral("profiles/")))
            continue;
        const QString rest = key.mid(QStringLiteral("profiles/").size());
        const int slash = rest.indexOf(u'/');
        if (slash <= 0)
            continue;
        idSet.insert(rest.left(slash));
    }
    QStringList ids = QStringList(idSet.begin(), idSet.end());
    ids.sort();

    for (const QString &id : ids) {
        const QString grp = QStringLiteral("profiles/") + id;
        SipProfile p;
        p.profileId   = id;
        p.displayName = m_settings.value(grp + "/displayName").toString();
        p.sipUsername = m_settings.value(grp + "/sipUsername").toString();
        p.sipDomain   = m_settings.value(grp + "/sipDomain").toString();
        p.sipUri      = m_settings.value(grp + "/sipUri").toString();
        p.registrar   = m_settings.value(grp + "/registrar").toString();
        p.proxy       = m_settings.value(grp + "/proxy").toString();
        p.outboundProxy     = m_settings.value(grp + "/outboundProxy").toString();
        p.authUsername      = m_settings.value(grp + "/authUsername").toString();
        p.emergencyServiceUri = m_settings.value(grp + "/emergencyServiceUri").toString();
        p.enableRtt              = m_settings.value(grp + "/enableRtt", false).toBool();
        p.enableLmpe             = m_settings.value(grp + "/enableLmpe", false).toBool();
        p.enableEtsiCompatibility = m_settings.value(grp + "/enableEtsiCompatibility", false).toBool();
        p.createdAt = QDateTime::fromString(m_settings.value(grp + "/createdAt").toString(), Qt::ISODate);
        p.updatedAt = QDateTime::fromString(m_settings.value(grp + "/updatedAt").toString(), Qt::ISODate);

        bool transportOk = false;
        p.transport = transportFromString(m_settings.value(grp + "/transport", "UDP").toString(), &transportOk);
        if (!transportOk) {
            Logger::instance().warn(LogCategory::Sip,
                QStringLiteral("Profile %1: unrecognised transport value, defaulting to UDP").arg(id));
        }

        m_profiles.append(p);
    }

    m_activeProfileId = m_settings.value("active/profileId").toString();
    // Verify the stored active id still exists; clear if not
    if (!m_activeProfileId.isEmpty() && !hasProfile(m_activeProfileId)) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Stored active profile id %1 not found; clearing").arg(m_activeProfileId));
        m_activeProfileId.clear();
        m_settings.remove("active");
    }
}

void SipProfileManager::saveProfile(const SipProfile &profile)
{
    const QString grp = QStringLiteral("profiles/") + profile.profileId;
    m_settings.setValue(grp + "/displayName",           profile.displayName);
    m_settings.setValue(grp + "/sipUsername",           profile.sipUsername);
    m_settings.setValue(grp + "/sipDomain",             profile.sipDomain);
    m_settings.setValue(grp + "/sipUri",                profile.sipUri);
    m_settings.setValue(grp + "/registrar",             profile.registrar);
    m_settings.setValue(grp + "/proxy",                 profile.proxy);
    m_settings.setValue(grp + "/outboundProxy",         profile.outboundProxy);
    m_settings.setValue(grp + "/transport",             transportToString(profile.transport));
    m_settings.setValue(grp + "/authUsername",          profile.authUsername);
    m_settings.setValue(grp + "/emergencyServiceUri",   profile.emergencyServiceUri);
    m_settings.setValue(grp + "/enableRtt",             profile.enableRtt);
    m_settings.setValue(grp + "/enableLmpe",            profile.enableLmpe);
    m_settings.setValue(grp + "/enableEtsiCompatibility", profile.enableEtsiCompatibility);
    m_settings.setValue(grp + "/createdAt",             profile.createdAt.toString(Qt::ISODate));
    m_settings.setValue(grp + "/updatedAt",             profile.updatedAt.toString(Qt::ISODate));
}

void SipProfileManager::removeFromStorage(const QString &profileId)
{
    m_settings.remove(QStringLiteral("profiles/") + profileId);
}
