#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include "sip/SipProfileManager.h"

// Isolated settings: never touches the real user SIPClientProfiles INI
static constexpr const char *kTestOrg = "IrymeTest";
static constexpr const char *kTestApp = "SIPClientTest_Profiles";
static QTemporaryDir *g_settingsDir = nullptr;

static void ensureTestSettingsPath()
{
    if (g_settingsDir)
        return;

    g_settingsDir = new QTemporaryDir;
    if (!g_settingsDir->isValid())
        qFatal("Failed to create temporary settings directory");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, g_settingsDir->path());
}

static void purgeTestSettings()
{
    ensureTestSettingsPath();
    QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
    s.clear();
    s.sync();
}

static SipProfile makeValid()
{
    SipProfile p = SipProfile::createNew();
    p.displayName = QStringLiteral("Alice");
    p.sipUsername = QStringLiteral("alice");
    p.sipDomain   = QStringLiteral("example.com");
    p.registrar   = QStringLiteral("registrar.example.com");
    return p;
}

class TestSipProfileManager : public QObject
{
    Q_OBJECT

private slots:
    void init()    { purgeTestSettings(); }
    void cleanup() { purgeTestSettings(); }

    // 1. Validation accepts a fully populated valid profile
    void validationAcceptsValidProfile()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const auto result = mgr.validate(makeValid());
        QVERIFY(result.valid);
        QVERIFY(result.errors.isEmpty());
    }

    // 2. Validation rejects a profile missing required fields
    void validationRejectsInvalidProfile()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);

        SipProfile missing;
        missing.profileId = QStringLiteral("dummy");
        // All required fields empty

        const auto result = mgr.validate(missing);
        QVERIFY(!result.valid);
        QVERIFY(result.errors.size() >= 4); // displayName, sipUsername, sipDomain, registrar
    }

    // 3. sipUri is derived from username@domain when left empty
    void sipUriDerived()
    {
        SipProfile p;
        p.sipUsername = QStringLiteral("bob");
        p.sipDomain   = QStringLiteral("sip.example.com");
        p.sipUri.clear();

        QCOMPARE(p.effectiveSipUri(), QStringLiteral("sip:bob@sip.example.com"));
    }

    // 4. sipUri is NOT overridden when explicitly set
    void sipUriExplicitlySetNotOverridden()
    {
        SipProfile p;
        p.sipUsername = QStringLiteral("bob");
        p.sipDomain   = QStringLiteral("sip.example.com");
        p.sipUri      = QStringLiteral("sip:bob@proxy.example.com");

        QCOMPARE(p.effectiveSipUri(), QStringLiteral("sip:bob@proxy.example.com"));
    }

    // 5. add/update/delete profile
    void addUpdateDeleteProfile()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);

        const QString id = mgr.add(makeValid());
        QVERIFY(!id.isEmpty());
        QVERIFY(mgr.hasProfile(id));
        QCOMPARE(mgr.profiles().size(), 1);

        SipProfile updated = mgr.profile(id);
        updated.displayName = QStringLiteral("Alice Updated");
        QVERIFY(mgr.update(updated));
        QCOMPARE(mgr.profile(id).displayName, QStringLiteral("Alice Updated"));

        QVERIFY(mgr.remove(id));
        QVERIFY(!mgr.hasProfile(id));
        QCOMPARE(mgr.profiles().size(), 0);
    }

    // 6. Active profile selection persists across manager instances (load/save round-trip)
    void activeProfileIdPersists()
    {
        {
            SipProfileManager mgr(kTestOrg, kTestApp);
            const QString id = mgr.add(makeValid());
            QVERIFY(!id.isEmpty());
            mgr.setActiveProfileId(id);
            QCOMPARE(mgr.activeProfileId(), id);
        }
        {
            // New instance reads from same isolated INI
            SipProfileManager mgr2(kTestOrg, kTestApp);
            QVERIFY(!mgr2.activeProfileId().isEmpty());
            QVERIFY(mgr2.hasActiveProfile());
        }
    }

    // 7. Full load/save round-trip preserves all non-secret fields
    void profileRoundTrip()
    {
        const QString id = [&]() {
            SipProfileManager mgr(kTestOrg, kTestApp);
            SipProfile p = makeValid();
            p.proxy             = QStringLiteral("proxy.example.com");
            p.outboundProxy     = QStringLiteral("ob.example.com");
            p.authUsername      = QStringLiteral("aliceauth");
            p.emergencyServiceUri = QStringLiteral("sip:112@psap.example");
            p.transport         = SipTransport::TLS;
            p.enableRtt         = true;
            p.enableLmpe        = false;
            p.enableEtsiCompatibility = true;
            return mgr.add(p);
        }();

        SipProfileManager mgr2(kTestOrg, kTestApp);
        QVERIFY(mgr2.hasProfile(id));
        const SipProfile loaded = mgr2.profile(id);
        QCOMPARE(loaded.displayName, QStringLiteral("Alice"));
        QCOMPARE(loaded.sipUsername, QStringLiteral("alice"));
        QCOMPARE(loaded.sipDomain,   QStringLiteral("example.com"));
        QCOMPARE(loaded.registrar,   QStringLiteral("registrar.example.com"));
        QCOMPARE(loaded.proxy,       QStringLiteral("proxy.example.com"));
        QCOMPARE(loaded.outboundProxy, QStringLiteral("ob.example.com"));
        QCOMPARE(loaded.authUsername, QStringLiteral("aliceauth"));
        QCOMPARE(loaded.emergencyServiceUri, QStringLiteral("sip:112@psap.example"));
        QCOMPARE(loaded.transport,   SipTransport::TLS);
        QVERIFY(loaded.enableRtt);
        QVERIFY(!loaded.enableLmpe);
        QVERIFY(loaded.enableEtsiCompatibility);
    }

    // Task W113F: LMPE has no interoperable wire format and must never load
    // as enabled, even if an old saved profile (or a hand-edited settings
    // file) has enableLmpe=true -- loadAllProfiles()/profile() must force it
    // false, not merely the UI checkbox that reads it.
    void enableLmpeForcedFalseOnLoadRegardlessOfSavedValue()
    {
        const QString id = [&]() {
            SipProfileManager mgr(kTestOrg, kTestApp);
            SipProfile p = makeValid();
            p.enableLmpe = true; // simulates stale/imported config with LMPE enabled
            return mgr.add(p);
        }();

        // Fresh manager instance -> forces a real reload from QSettings,
        // exercising the same load path a restarted application would use.
        SipProfileManager mgr2(kTestOrg, kTestApp);
        const SipProfile loaded = mgr2.profile(id);
        QVERIFY2(!loaded.enableLmpe,
            "enableLmpe must be forced false on load even if saved as true");
    }

    // 8. No password or secret fields are stored in the settings file
    void noPasswordFieldsStored()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        mgr.add(makeValid());
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
                    qPrintable(QStringLiteral("Forbidden key found: ") + key));
            }
        }
    }

    // 9. Invalid transport string falls back to UDP (corrupt INI tolerance)
    void invalidTransportFallsBackToUdp()
    {
        bool ok = true;
        const SipTransport t = SipProfileManager::transportFromString("GARBAGE", &ok);
        QVERIFY(!ok);
        QCOMPARE(t, SipTransport::UDP);
    }

    // 10. Removing active profile clears the active selection
    void removingActiveProfileClearsSelection()
    {
        SipProfileManager mgr(kTestOrg, kTestApp);
        const QString id = mgr.add(makeValid());
        mgr.setActiveProfileId(id);
        QVERIFY(mgr.hasActiveProfile());

        mgr.remove(id);
        QVERIFY(!mgr.hasActiveProfile());
        QVERIFY(mgr.activeProfileId().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSipProfileManager)
#include "test_sip_profile_manager.moc"
