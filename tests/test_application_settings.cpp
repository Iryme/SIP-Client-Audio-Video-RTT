#include <QtTest>
#include "settings/ApplicationSettings.h"

// Unique org/app names so these tests never touch real user settings.
static constexpr const char *kTestOrg = "IrymeTest";
static constexpr const char *kTestApp = "SIPClientTest_Settings";

// Helper: clean up the test QSettings file between test cases.
static void purgeTestSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
    s.clear();
    s.sync();
}

class TestApplicationSettings : public QObject
{
    Q_OBJECT

private slots:
    void init()    { purgeTestSettings(); }
    void cleanup() { purgeTestSettings(); }

    // 1. Default values are correct on first read (no prior settings file).
    void defaultValues()
    {
        ApplicationSettings s(kTestOrg, kTestApp);

        QCOMPARE(s.diagLevelEnabled(LogLevel::Info),  true);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Warn),  true);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Error), true);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Debug), false);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Raw),   false);

        QCOMPARE(s.theme(), QString("dark"));
        QVERIFY(s.windowGeometry().isEmpty());
        QVERIFY(s.splitterState("horizontal").isEmpty());
        QVERIFY(s.selectedSipProfileId().isEmpty());
        QVERIFY(s.selectedMicrophoneId().isEmpty());
        QVERIFY(s.selectedSpeakerId().isEmpty());
        QVERIFY(s.selectedCameraId().isEmpty());
    }

    // 2. Saving and reloading diagnostics level states round-trips correctly.
    void saveAndReloadDiagLevels()
    {
        {
            ApplicationSettings s(kTestOrg, kTestApp);
            s.setDiagLevelEnabled(LogLevel::Info,  false);
            s.setDiagLevelEnabled(LogLevel::Warn,  false);
            s.setDiagLevelEnabled(LogLevel::Error, true);
            s.setDiagLevelEnabled(LogLevel::Debug, true);
            s.setDiagLevelEnabled(LogLevel::Raw,   true);
            s.sync();
        }

        ApplicationSettings s2(kTestOrg, kTestApp);
        QCOMPARE(s2.diagLevelEnabled(LogLevel::Info),  false);
        QCOMPARE(s2.diagLevelEnabled(LogLevel::Warn),  false);
        QCOMPARE(s2.diagLevelEnabled(LogLevel::Error), true);
        QCOMPARE(s2.diagLevelEnabled(LogLevel::Debug), true);
        QCOMPARE(s2.diagLevelEnabled(LogLevel::Raw),   true);
    }

    // 3. RAW is disabled by default — never enabled by accident.
    void rawDefaultDisabled()
    {
        ApplicationSettings s(kTestOrg, kTestApp);
        // Fresh settings: RAW must be off.
        QCOMPARE(s.diagLevelEnabled(LogLevel::Raw), false);

        // After reset: RAW must be off.
        s.setDiagLevelEnabled(LogLevel::Raw, true);
        s.sync();
        s.resetToDefaults();
        QCOMPARE(s.diagLevelEnabled(LogLevel::Raw), false);
    }

    // 4. resetToDefaults restores all levels to their factory values.
    void resetToDefaults()
    {
        ApplicationSettings s(kTestOrg, kTestApp);
        s.setDiagLevelEnabled(LogLevel::Info,  false);
        s.setDiagLevelEnabled(LogLevel::Debug, true);
        s.setDiagLevelEnabled(LogLevel::Raw,   true);
        s.setTheme("light");
        s.setSelectedSipProfileId("profile-1");
        s.sync();

        s.resetToDefaults();

        QCOMPARE(s.diagLevelEnabled(LogLevel::Info),  true);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Warn),  true);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Error), true);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Debug), false);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Raw),   false);
        QCOMPARE(s.theme(), QString("dark"));
        QVERIFY(s.selectedSipProfileId().isEmpty());
    }

    // 5. Missing or unrecognised values fall back safely to defaults.
    void missingValuesFallback()
    {
        // Write a garbage value for the debug level key directly into QSettings.
        {
            QSettings raw(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kTestApp);
            raw.setValue("diagnostics/level/debug", "garbage_value");
            raw.setValue("diagnostics/level/raw",   "garbage_value");
            raw.sync();
        }

        ApplicationSettings s(kTestOrg, kTestApp);
        // toBool() on "garbage_value" returns false — same as the safe default.
        QCOMPARE(s.diagLevelEnabled(LogLevel::Debug), false);
        QCOMPARE(s.diagLevelEnabled(LogLevel::Raw),   false);
    }

    // 6. No credential fields are stored — iterate all written keys.
    void noCredentialFields()
    {
        static constexpr const char *kCredApp = "SIPClientTest_Cred";

        QSettings purge(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kCredApp);
        purge.clear();
        purge.sync();

        {
            ApplicationSettings s(kTestOrg, kCredApp);
            s.setDiagLevelEnabled(LogLevel::Info,    true);
            s.setSelectedSipProfileId("profile-abc");
            s.setSelectedMicrophoneId("mic-1");
            s.setSelectedSpeakerId("spk-1");
            s.setSelectedCameraId("cam-1");
            s.setTheme("dark");
            s.setWindowGeometry(QByteArray("fakegeom"));
            s.sync();
        }

        QSettings raw(QSettings::IniFormat, QSettings::UserScope, kTestOrg, kCredApp);
        const QStringList keys = raw.allKeys();
        QVERIFY2(!keys.isEmpty(), "Expected at least one key to be written");

        const QStringList forbidden = {"password", "passwd", "secret", "token",
                                       "credential", "auth", "key", "pin"};
        for (const QString &k : keys) {
            for (const QString &banned : forbidden) {
                QVERIFY2(!k.contains(banned, Qt::CaseInsensitive),
                         qPrintable(QString("Credential field found in settings: %1").arg(k)));
            }
        }

        // Cleanup
        purge.clear();
        purge.sync();
    }

    // 7. Category filter round-trips correctly.
    void categoryFilterPersists()
    {
        {
            ApplicationSettings s(kTestOrg, kTestApp);
            s.setDiagCategoryFilter("SIP");
            s.sync();
        }
        ApplicationSettings s2(kTestOrg, kTestApp);
        QCOMPARE(s2.diagCategoryFilter(), QString("SIP"));
    }

    // 8. Placeholder device IDs round-trip correctly.
    void deviceIdsPersist()
    {
        {
            ApplicationSettings s(kTestOrg, kTestApp);
            s.setSelectedMicrophoneId("mic-device-1");
            s.setSelectedSpeakerId("spk-device-2");
            s.setSelectedCameraId("cam-device-3");
            s.sync();
        }
        ApplicationSettings s2(kTestOrg, kTestApp);
        QCOMPARE(s2.selectedMicrophoneId(), QString("mic-device-1"));
        QCOMPARE(s2.selectedSpeakerId(),    QString("spk-device-2"));
        QCOMPARE(s2.selectedCameraId(),     QString("cam-device-3"));
    }
};

QTEST_GUILESS_MAIN(TestApplicationSettings)
#include "test_application_settings.moc"
