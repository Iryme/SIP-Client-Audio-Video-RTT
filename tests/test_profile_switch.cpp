#include <QtTest/QtTest>

#include "core/Logger.h"
#include "security/CredentialStore.h"
#include "security/MemoryCredentialBackend.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

class TestProfileSwitch : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Fast path (Unregistered / RegistrationFailed → switch immediately)
    void switchWhileUnregisteredRegistersNewProfile();
    void switchToEmptyProfileWhileUnregistered();
    void switchWhileRegistrationFailedSwitchesImmediately();

    // Slow path (Registering / Registered → unregister old first)
    void switchWhileRegisteringUnregistersOldFirst();
    void newProfileNotRegisteredBeforeOldCleanup();
    void secondSwitchRejectedWhileFirstPending();

    // Shutdown
    void shutdownCancelsPendingSwitch();

    // Password safety
    void switchDoesNotLogPassword();

private:
    QString addProfileWithPassword(const QString &displayName, const QString &password);

    QStringList m_createdProfileIds;
    QString     m_originalActiveProfileId;
};

// ---- helpers ----------------------------------------------------------------

void TestProfileSwitch::init()
{
    SipManager::instance().shutdown();
    m_originalActiveProfileId = SipProfileManager::instance().activeProfileId();
    CredentialStore::instance().setBackend(std::make_unique<MemoryCredentialBackend>());

    // Use a long retry delay so that retry timers never fire unintentionally.
    RegistrationRetryPolicy p;
    p.maxAttempts    = 3;
    p.initialDelayMs = 30000;
    p.maxDelayMs     = 60000;
    SipManager::instance().setRetryPolicy(p);
}

void TestProfileSwitch::cleanup()
{
    SipManager::instance().shutdown();
    for (const QString &id : m_createdProfileIds)
        SipProfileManager::instance().remove(id);
    m_createdProfileIds.clear();
    SipProfileManager::instance().setActiveProfileId(m_originalActiveProfileId);
}

QString TestProfileSwitch::addProfileWithPassword(const QString &displayName,
                                                  const QString &password)
{
    SipProfile profile = SipProfile::createNew();
    profile.displayName  = displayName;
    profile.sipUsername  = displayName.toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
    profile.sipDomain    = QStringLiteral("example.invalid");
    profile.registrar    = QStringLiteral("example.invalid");
    profile.authUsername = profile.sipUsername + QStringLiteral("-auth");

    const QString id = SipProfileManager::instance().add(profile);
    if (id.isEmpty())
        return {};
    m_createdProfileIds << id;
    SipProfileManager::instance().setProfilePassword(id, password);
    return id;
}

// ---- fast path tests --------------------------------------------------------

void TestProfileSwitch::switchWhileUnregisteredRegistersNewProfile()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString idB = addProfileWithPassword(QStringLiteral("Profile B"),
                                               QStringLiteral("secret-b"));
    QVERIFY(!idB.isEmpty());

    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Unregistered);

    QSignalSpy completedSpy(&SipManager::instance(), &SipManager::profileSwitchCompleted);
    QSignalSpy startedSpy  (&SipManager::instance(), &SipManager::profileSwitchStarted);

    QVERIFY(SipManager::instance().switchActiveProfile(idB));

    // Fast path: profileSwitchStarted is NOT emitted, only profileSwitchCompleted.
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy[0][0].toString(), idB);
    QCOMPARE(SipProfileManager::instance().activeProfileId(), idB);

    // Registration is triggered (stub mode → eventually RegistrationFailed).
    QTRY_COMPARE_WITH_TIMEOUT(SipManager::instance().registrationState(),
                               RegistrationState::RegistrationFailed, 2000);
}

void TestProfileSwitch::switchToEmptyProfileWhileUnregistered()
{
    SipProfileManager::instance().setActiveProfileId({});
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Unregistered);

    QSignalSpy completedSpy(&SipManager::instance(), &SipManager::profileSwitchCompleted);

    QVERIFY(SipManager::instance().switchActiveProfile({}));

    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy[0][0].toString(), QString{});
    QCOMPARE(SipProfileManager::instance().activeProfileId(), QString{});
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Unregistered);
}

void TestProfileSwitch::switchWhileRegistrationFailedSwitchesImmediately()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString idA = addProfileWithPassword(QStringLiteral("Profile A"),
                                               QStringLiteral("secret-a"));
    QVERIFY(!idA.isEmpty());
    SipProfileManager::instance().setActiveProfileId(idA);

    // Reach RegistrationFailed.
    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE_WITH_TIMEOUT(SipManager::instance().registrationState(),
                               RegistrationState::RegistrationFailed, 2000);

    const QString idB = addProfileWithPassword(QStringLiteral("Profile B"),
                                               QStringLiteral("secret-b"));
    QVERIFY(!idB.isEmpty());

    QSignalSpy startedSpy   (&SipManager::instance(), &SipManager::profileSwitchStarted);
    QSignalSpy completedSpy (&SipManager::instance(), &SipManager::profileSwitchCompleted);

    QVERIFY(SipManager::instance().switchActiveProfile(idB));

    // Fast path — no pending needed.
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy[0][0].toString(), idB);
    QCOMPARE(SipProfileManager::instance().activeProfileId(), idB);
    QVERIFY(!SipManager::instance().isSwitchingProfile());
}

// ---- slow path tests --------------------------------------------------------

void TestProfileSwitch::switchWhileRegisteringUnregistersOldFirst()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString idA = addProfileWithPassword(QStringLiteral("Alpha"),
                                               QStringLiteral("alpha-pw"));
    const QString idB = addProfileWithPassword(QStringLiteral("Beta"),
                                               QStringLiteral("beta-pw"));
    QVERIFY(!idA.isEmpty() && !idB.isEmpty());

    SipProfileManager::instance().setActiveProfileId(idA);
    SipManager::instance().registerActiveProfile();
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Registering);

    QSignalSpy startedSpy   (&SipManager::instance(), &SipManager::profileSwitchStarted);
    QSignalSpy completedSpy (&SipManager::instance(), &SipManager::profileSwitchCompleted);

    // Call switch while Registering → slow path.
    QVERIFY(SipManager::instance().switchActiveProfile(idB));
    QVERIFY(SipManager::instance().isSwitchingProfile());
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy[0][0].toString(), idB);

    // Wait for the whole sequence: Unregistering → Unregistered → Registering (B) → RegistrationFailed (B).
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 3000);
    QCOMPARE(completedSpy[0][0].toString(), idB);
    QVERIFY(!SipManager::instance().isSwitchingProfile());
    QCOMPARE(SipProfileManager::instance().activeProfileId(), idB);
}

void TestProfileSwitch::newProfileNotRegisteredBeforeOldCleanup()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString idA = addProfileWithPassword(QStringLiteral("A-Profile"),
                                               QStringLiteral("a-secret"));
    const QString idB = addProfileWithPassword(QStringLiteral("B-Profile"),
                                               QStringLiteral("b-secret"));
    QVERIFY(!idA.isEmpty() && !idB.isEmpty());

    SipProfileManager::instance().setActiveProfileId(idA);
    SipManager::instance().registerActiveProfile();
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Registering);

    QSignalSpy startedSpy   (&SipManager::instance(), &SipManager::profileSwitchStarted);
    QSignalSpy completedSpy (&SipManager::instance(), &SipManager::profileSwitchCompleted);

    QVERIFY(SipManager::instance().switchActiveProfile(idB));
    QCOMPARE(startedSpy.count(), 1);

    // Immediately after profileSwitchStarted, the active profile must still be A
    // (the new account has NOT been created yet).
    QCOMPARE(SipProfileManager::instance().activeProfileId(), idA);
    QVERIFY(SipManager::instance().isSwitchingProfile());

    // Wait for completion — only then is the active profile updated.
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 3000);
    QCOMPARE(SipProfileManager::instance().activeProfileId(), idB);
}

void TestProfileSwitch::secondSwitchRejectedWhileFirstPending()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString idA = addProfileWithPassword(QStringLiteral("A"),
                                               QStringLiteral("pw-a"));
    const QString idB = addProfileWithPassword(QStringLiteral("B"),
                                               QStringLiteral("pw-b"));
    const QString idC = addProfileWithPassword(QStringLiteral("C"),
                                               QStringLiteral("pw-c"));
    QVERIFY(!idA.isEmpty() && !idB.isEmpty() && !idC.isEmpty());

    SipProfileManager::instance().setActiveProfileId(idA);
    SipManager::instance().registerActiveProfile();
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Registering);

    QSignalSpy completedSpy(&SipManager::instance(), &SipManager::profileSwitchCompleted);
    QSignalSpy failedSpy   (&SipManager::instance(), &SipManager::profileSwitchFailed);

    QVERIFY(SipManager::instance().switchActiveProfile(idB));  // slow path — pending
    QVERIFY(SipManager::instance().isSwitchingProfile());

    // Second switch must be rejected.
    QVERIFY(!SipManager::instance().switchActiveProfile(idC));

    // Wait for the first switch to complete (B).
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 3000);
    QCOMPARE(completedSpy[0][0].toString(), idB);
    QCOMPARE(failedSpy.count(), 0);   // no failure signals
}

// ---- shutdown / cancel ------------------------------------------------------

void TestProfileSwitch::shutdownCancelsPendingSwitch()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString idA = addProfileWithPassword(QStringLiteral("ShutA"),
                                               QStringLiteral("shut-a-pw"));
    const QString idB = addProfileWithPassword(QStringLiteral("ShutB"),
                                               QStringLiteral("shut-b-pw"));
    QVERIFY(!idA.isEmpty() && !idB.isEmpty());

    SipProfileManager::instance().setActiveProfileId(idA);
    SipManager::instance().registerActiveProfile();
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Registering);

    QVERIFY(SipManager::instance().switchActiveProfile(idB));
    QVERIFY(SipManager::instance().isSwitchingProfile());

    QSignalSpy failedSpy(&SipManager::instance(), &SipManager::profileSwitchFailed);

    SipManager::instance().shutdown();

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy[0][0].toString(), idB);
    QVERIFY(!SipManager::instance().isSwitchingProfile());
}

// ---- password safety --------------------------------------------------------

void TestProfileSwitch::switchDoesNotLogPassword()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only");
#endif
    const QString secretA = QStringLiteral("NEVER-LOG-SWITCH-SECRET-ALPHA");
    const QString secretB = QStringLiteral("NEVER-LOG-SWITCH-SECRET-BETA");

    const QString idA = addProfileWithPassword(QStringLiteral("LogA"), secretA);
    const QString idB = addProfileWithPassword(QStringLiteral("LogB"), secretB);
    QVERIFY(!idA.isEmpty() && !idB.isEmpty());

    SipProfileManager::instance().setActiveProfileId(idA);
    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE_WITH_TIMEOUT(SipManager::instance().registrationState(),
                               RegistrationState::RegistrationFailed, 2000);

    QStringList messages;
    const auto conn = connect(
        &Logger::instance(), &Logger::entryAdded, this,
        [&messages](const LogEntry &entry) {
            messages << entry.message << entry.payload;
        });

    // Fast-path switch (RegistrationFailed → switch immediately).
    SipManager::instance().switchActiveProfile(idB);
    QTRY_COMPARE_WITH_TIMEOUT(SipManager::instance().registrationState(),
                               RegistrationState::RegistrationFailed, 2000);

    disconnect(conn);

    for (const QString &msg : messages) {
        QVERIFY2(!msg.contains(secretA), "Password A appeared in diagnostics during switch");
        QVERIFY2(!msg.contains(secretB), "Password B appeared in diagnostics during switch");
    }
}

QTEST_GUILESS_MAIN(TestProfileSwitch)
#include "test_profile_switch.moc"
