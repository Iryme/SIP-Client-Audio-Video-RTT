#include <QtTest/QtTest>
#include <memory>

#include "core/Logger.h"
#include "security/CredentialStore.h"
#include "security/MemoryCredentialBackend.h"
#include "sip/SipProfile.h"
#include "sip/SipProfileManager.h"

// All tests run against CredentialStore::instance() with a fresh MemoryCredentialBackend
// injected via setBackend() before each test. The OS keychain is NEVER touched.
//
// SipProfileManager tests use an isolated QSettings namespace.

static constexpr const char *kTestOrg = "IrymeTest";
static constexpr const char *kTestApp = "SIPClientTest_Credentials";

static void purgeTestSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
    s.clear();
    s.sync();
}

static SipProfile makeProfile()
{
    SipProfile p = SipProfile::createNew();
    p.displayName = QStringLiteral("Alice");
    p.sipUsername = QStringLiteral("alice");
    p.sipDomain   = QStringLiteral("sip.example.com");
    p.registrar   = QStringLiteral("sip.example.com");
    return p;
}

// Convenience: returns the singleton wired with a fresh MemoryCredentialBackend.
// The raw pointer is non-owning; the unique_ptr was moved into the store.
static CredentialStore &freshStore()
{
    CredentialStore::instance().setBackend(std::make_unique<MemoryCredentialBackend>());
    return CredentialStore::instance();
}

class TestCredentialStore : public QObject
{
    Q_OBJECT

private slots:
    void init()    { purgeTestSettings(); freshStore(); }
    void cleanup() { purgeTestSettings(); }

    // 1. Store then load returns the original password.
    void saveAndLoadPassword()
    {
        auto &cs = CredentialStore::instance();
        const QString id   = QStringLiteral("profile-001");
        const QString user = QStringLiteral("alice");
        const QString pass = QStringLiteral("s3cr3t!");

        QVERIFY(cs.storePassword(id, user, pass));

        bool found = false;
        const QString loaded = cs.loadPassword(id, user, &found);
        QVERIFY(found);
        QCOMPARE(loaded, pass);
    }

    // 2. Delete removes the credential; subsequent load returns not-found.
    void deletePasswordRemovesCredential()
    {
        auto &cs = CredentialStore::instance();
        const QString id   = QStringLiteral("profile-002");
        const QString user = QStringLiteral("bob");

        cs.storePassword(id, user, QStringLiteral("abc123"));
        QVERIFY(cs.hasPassword(id, user));

        QVERIFY(cs.deletePassword(id, user));

        bool found = true;
        cs.loadPassword(id, user, &found);
        QVERIFY(!found);
        QVERIFY(!cs.hasPassword(id, user));
    }

    // 3. hasPassword returns false before store, true after.
    void hasPasswordTrueAndFalse()
    {
        auto &cs = CredentialStore::instance();
        const QString id   = QStringLiteral("profile-003");
        const QString user = QStringLiteral("carol");

        QVERIFY(!cs.hasPassword(id, user));
        cs.storePassword(id, user, QStringLiteral("pw"));
        QVERIFY(cs.hasPassword(id, user));
    }

    // 4. Storing twice overwrites — load returns the second value.
    void overwritePassword()
    {
        auto &cs = CredentialStore::instance();
        const QString id   = QStringLiteral("profile-004");
        const QString user = QStringLiteral("dave");

        cs.storePassword(id, user, QStringLiteral("first"));
        cs.storePassword(id, user, QStringLiteral("second"));

        bool found = false;
        QCOMPARE(cs.loadPassword(id, user, &found), QStringLiteral("second"));
        QVERIFY(found);
    }

    // 5. Loading a non-existent credential: found=false, returns empty.
    void credentialNotFound()
    {
        auto &cs = CredentialStore::instance();
        bool found = true;
        const QString val = cs.loadPassword(
            QStringLiteral("no-such-profile"), QStringLiteral("nobody"), &found);
        QVERIFY(!found);
        QVERIFY(val.isEmpty());
    }

    // 6. Two profiles with different IDs stored separately; each loads its own value.
    void multipleProfiles()
    {
        auto &cs = CredentialStore::instance();
        cs.storePassword(QStringLiteral("p1"), QStringLiteral("u"), QStringLiteral("pw-one"));
        cs.storePassword(QStringLiteral("p2"), QStringLiteral("u"), QStringLiteral("pw-two"));

        bool f1 = false, f2 = false;
        QCOMPARE(cs.loadPassword(QStringLiteral("p1"), QStringLiteral("u"), &f1),
                 QStringLiteral("pw-one"));
        QCOMPARE(cs.loadPassword(QStringLiteral("p2"), QStringLiteral("u"), &f2),
                 QStringLiteral("pw-two"));
        QVERIFY(f1 && f2);
    }

    // 7. backendName() and isSecureBackendAvailable() reflect the injected backend.
    void backendNameReported()
    {
        auto &cs = CredentialStore::instance();
        QCOMPARE(cs.backendName(), QStringLiteral("Memory (test-only, NOT secure)"));
        QVERIFY(!cs.isSecureBackendAvailable());
    }

    // 8. Deleting a non-existent credential returns false but does not crash.
    void deleteNonExistentIsGraceful()
    {
        auto &cs = CredentialStore::instance();
        const bool ok = cs.deletePassword(
            QStringLiteral("ghost-profile"), QStringLiteral("ghost"));
        QVERIFY(!ok);
    }

    // 9. No password stored in SipProfile QSettings after saving a profile.
    //    Scans all QSettings keys and values for forbidden substrings.
    void noPasswordInSipProfilePersistence()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        mgr.add(makeProfile());
        mgr.sync();

        QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
        const QStringList forbidden = {
            "password", "secret", "token", "credential", "passwd", "auth_pass"
        };
        for (const QString &key : s.allKeys()) {
            const QString lkey = key.toLower();
            for (const QString &word : forbidden) {
                QVERIFY2(!lkey.contains(word),
                    qPrintable(QStringLiteral("Forbidden key in QSettings: '%1'").arg(key)));
            }
            const QString val = s.value(key).toString().toLower();
            for (const QString &word : forbidden) {
                QVERIFY2(!val.contains(word),
                    qPrintable(QStringLiteral(
                        "Forbidden word '%1' in QSettings value for key '%2'")
                            .arg(word, key)));
            }
        }
    }

    // 10. No password value appears in Logger output after storePassword.
    void noPasswordInLogOutput()
    {
        QStringList logMessages;
        QMetaObject::Connection conn = connect(
            &Logger::instance(), &Logger::entryAdded,
            [&logMessages](const LogEntry &e) {
                logMessages << e.message << e.payload;
            });

        auto &cs = CredentialStore::instance();
        const QString pass = QStringLiteral("SuperSecret42!");
        cs.storePassword(QStringLiteral("p-log"), QStringLiteral("user"), pass);

        disconnect(conn);

        for (const QString &msg : logMessages) {
            QVERIFY2(!msg.contains(pass),
                qPrintable(QStringLiteral(
                    "Password value leaked into log: '%1'").arg(msg)));
        }
    }
};

QTEST_GUILESS_MAIN(TestCredentialStore)
#include "test_credential_store.moc"
