#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QSettings>
#include "sip/SipProfile.h"

struct ProfileValidationResult {
    bool        valid{false};
    QStringList errors;
};

class SipProfileManager : public QObject
{
    Q_OBJECT
public:
    static SipProfileManager &instance();

    // Task W109A — lets two instances of this application on the same host
    // use separate profile stores instead of silently sharing the single
    // per-Windows-user "SIPClientProfiles" ini. Must be called before the
    // first call to instance() to have any effect. See --config-dir / the
    // SIPCLIENT_CONFIG_DIR environment variable in main.cpp and
    // AppSettings::setConfigDirectoryOverride() (same mechanism, applied to
    // the separate profile store).
    static void setConfigDirectoryOverride(const QString &dir);

    // Testable constructor: pass custom org/app to isolate test settings
    explicit SipProfileManager(const QString &org, const QString &app,
                                QSettings::Format format = QSettings::IniFormat,
                                QObject *parent = nullptr);

    // Explicit-file-path constructor, used when a config directory override
    // is set (see setConfigDirectoryOverride()).
    explicit SipProfileManager(const QString &iniFilePath, QSettings::Format format,
                                QObject *parent);

    ProfileValidationResult validate(const SipProfile &profile) const;

    // Returns the assigned profileId on success, empty string on validation failure
    QString add(SipProfile profile);
    bool    update(const SipProfile &profile);
    bool    remove(const QString &profileId);

    QList<SipProfile> profiles() const;
    SipProfile        profile(const QString &profileId) const;
    bool              hasProfile(const QString &profileId) const;

    void       setActiveProfileId(const QString &id);
    QString    activeProfileId() const;
    bool       hasActiveProfile() const;
    SipProfile activeProfile() const;

    void sync();

    // Credential helpers — delegate to CredentialStore.
    // Username is derived from authUsername if set, otherwise sipUsername.
    bool setProfilePassword(const QString &profileId, const QString &password);
    bool hasProfilePassword(const QString &profileId) const;
    bool removeProfilePassword(const QString &profileId);

    static QString      transportToString(SipTransport t);
    static SipTransport transportFromString(const QString &s, bool *ok = nullptr);

signals:
    void profileAdded(const QString &profileId);
    void profileUpdated(const QString &profileId);
    void profileRemoved(const QString &profileId);
    void activeProfileChanged(const QString &profileId);

private:
    void loadAll();
    void saveProfile(const SipProfile &profile);
    void removeFromStorage(const QString &profileId);

    QSettings         m_settings;
    QList<SipProfile> m_profiles;
    QString           m_activeProfileId;
};
