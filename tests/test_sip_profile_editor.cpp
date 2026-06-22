#include <QtTest>
#include <QSettings>
#include "sip/SipProfileManager.h"
#include "security/CredentialStore.h"
#include "security/MemoryCredentialBackend.h"

// Isolated settings namespaces — never touch real user data
static constexpr const char *kTestOrg = "IrymeTest";
static constexpr const char *kTestApp = "SIPClientTest_ProfileEditor";

static void purgeTestSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
    s.clear();
    s.sync();
}

static void injectMemoryBackend()
{
    CredentialStore::instance().setBackend(
        std::make_unique<MemoryCredentialBackend>());
}

static SipProfile makeValid(const QString &name = QStringLiteral("Test User"))
{
    SipProfile p = SipProfile::createNew();
    p.displayName = name;
    p.sipUsername = QStringLiteral("testuser");
    p.sipDomain   = QStringLiteral("example.com");
    p.registrar   = QStringLiteral("registrar.example.com");
    return p;
}

class TestSipProfileEditor : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        purgeTestSettings();
        injectMemoryBackend();
    }

    void cleanup()
    {
        purgeTestSettings();
    }

    // 1. Create profile — add a valid profile, verify it is stored and retrievable
    void createProfile()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const SipProfile p = makeValid(QStringLiteral("Carol"));
        const QString id = mgr.add(p);

        QVERIFY(!id.isEmpty());
        QVERIFY(mgr.hasProfile(id));
        QCOMPARE(mgr.profile(id).displayName, QStringLiteral("Carol"));
        QCOMPARE(mgr.profiles().size(), 1);
    }

    // 2. Edit profile — update an existing profile, verify changes are persisted
    void editProfile()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid(QStringLiteral("Original")));
        QVERIFY(!id.isEmpty());

        SipProfile updated = mgr.profile(id);
        updated.displayName = QStringLiteral("Updated");
        updated.proxy = QStringLiteral("proxy.example.com");
        QVERIFY(mgr.update(updated));

        const SipProfile loaded = mgr.profile(id);
        QCOMPARE(loaded.displayName, QStringLiteral("Updated"));
        QCOMPARE(loaded.proxy, QStringLiteral("proxy.example.com"));
    }

    // 3. Delete profile — remove a profile, verify it is no longer present
    void deleteProfile()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid());
        QVERIFY(!id.isEmpty());
        QVERIFY(mgr.hasProfile(id));

        QVERIFY(mgr.remove(id));
        QVERIFY(!mgr.hasProfile(id));
        QCOMPARE(mgr.profiles().size(), 0);
    }

    // 4. Password stored through CredentialStore — setProfilePassword stores;
    //    hasProfilePassword confirms; loadPassword retrieves correct value
    void passwordStoredThroughCredentialStore()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid());
        QVERIFY(!id.isEmpty());

        QVERIFY(!mgr.hasProfilePassword(id));
        QVERIFY(mgr.setProfilePassword(id, QStringLiteral("secret123")));
        QVERIFY(mgr.hasProfilePassword(id));

        // Load directly from CredentialStore to confirm value is there
        const SipProfile p = mgr.profile(id);
        const QString user = p.authUsername.isEmpty() ? p.sipUsername : p.authUsername;
        bool found = false;
        const QString loaded =
            CredentialStore::instance().loadPassword(id, user, &found);
        QVERIFY(found);
        QCOMPARE(loaded, QStringLiteral("secret123"));
    }

    // 5. Password not persisted in profile storage — QSettings scan must contain
    //    no password-related key names after saving a profile with a credential
    void passwordNotPersistedInProfileStorage()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid());
        mgr.setProfilePassword(id, QStringLiteral("s3cr3t"));
        mgr.sync();

        QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
        const QStringList allKeys = s.allKeys();
        const QStringList forbidden = {
            "password", "passwd", "secret", "token", "credential",
            "authpassword", "auth_password", "sippassword"
        };
        for (const QString &key : allKeys) {
            const QString lower = key.toLower();
            for (const QString &f : forbidden) {
                QVERIFY2(!lower.contains(f),
                    qPrintable(QStringLiteral("Forbidden key in profile storage: ") + key));
            }
        }
    }

    // 6. Validation errors — validate() returns errors for missing required fields
    void validationErrors()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);

        SipProfile bad;
        bad.profileId = QStringLiteral("dummy");
        // All required fields empty — should produce at least 4 errors
        const auto result = mgr.validate(bad);
        QVERIFY(!result.valid);
        QVERIFY(result.errors.size() >= 4);
    }

    // 7. Active profile update after edit — update the active profile, verify
    //    activeProfile() reflects the new data without reloading
    void activeProfileUpdateAfterEdit()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid(QStringLiteral("Before Edit")));
        mgr.setActiveProfileId(id);
        QVERIFY(mgr.hasActiveProfile());
        QCOMPARE(mgr.activeProfile().displayName, QStringLiteral("Before Edit"));

        SipProfile updated = mgr.profile(id);
        updated.displayName = QStringLiteral("After Edit");
        QVERIFY(mgr.update(updated));

        QCOMPARE(mgr.activeProfile().displayName, QStringLiteral("After Edit"));
    }

    // 8. Delete removes credential — remove() must also delete the stored password
    void deleteRemovesCredential()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid());
        QVERIFY(!id.isEmpty());

        QVERIFY(mgr.setProfilePassword(id, QStringLiteral("p@ssw0rd")));
        QVERIFY(mgr.hasProfilePassword(id));

        QVERIFY(mgr.remove(id));
        QVERIFY(!mgr.hasProfile(id));
        QVERIFY(!mgr.hasProfilePassword(id));
    }
};

QTEST_GUILESS_MAIN(TestSipProfileEditor)
#include "test_sip_profile_editor.moc"
