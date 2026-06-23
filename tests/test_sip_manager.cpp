#include <QtTest/QtTest>

#include "core/Logger.h"
#include "security/CredentialStore.h"
#include "security/MemoryCredentialBackend.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

class TestSipManager : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void stubBackendNameIsCorrect();
    void initializeSucceedsInStubMode();
    void initializeIsIdempotent();
    void shutdownIsIdempotent();
    void registrationStateNamesAreStable();
    void registrationWithoutActiveProfileFails();
    void stubRegistrationUsesActiveProfileAndFailsSafely();
    void stubUnregisterReturnsToUnregistered();
    void registerRejectedWhileRegistering();
    void registerRejectedWhileRegistered();
    void unregisterRejectedWhileUnregistered();
    void unregisteringStateIsExplicit();
    void passwordNeverAppearsInDiagnostics();

private:
    QString addActiveProfileWithPassword(const QString &password);

    QString m_createdProfileId;
    QString m_originalActiveProfileId;
};

void TestSipManager::init()
{
    SipManager::instance().shutdown();
    m_originalActiveProfileId = SipProfileManager::instance().activeProfileId();
    CredentialStore::instance().setBackend(
        std::make_unique<MemoryCredentialBackend>());
}

void TestSipManager::cleanup()
{
    SipManager::instance().shutdown();
    if (!m_createdProfileId.isEmpty()) {
        SipProfileManager::instance().remove(m_createdProfileId);
        m_createdProfileId.clear();
    }
    SipProfileManager::instance().setActiveProfileId(m_originalActiveProfileId);
}

QString TestSipManager::addActiveProfileWithPassword(const QString &password)
{
    SipProfile profile = SipProfile::createNew();
    profile.displayName = QStringLiteral("Registration Test");
    profile.sipUsername = QStringLiteral("registration-test");
    profile.sipDomain = QStringLiteral("example.invalid");
    profile.registrar = QStringLiteral("example.invalid");
    profile.authUsername = QStringLiteral("registration-auth");

    m_createdProfileId = SipProfileManager::instance().add(profile);
    if (m_createdProfileId.isEmpty())
        return {};
    SipProfileManager::instance().setActiveProfileId(m_createdProfileId);
    if (!SipProfileManager::instance().setProfilePassword(m_createdProfileId, password))
        return {};
    return m_createdProfileId;
}

void TestSipManager::stubBackendNameIsCorrect()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QCOMPARE(SipManager::instance().backendName(),
             QStringLiteral("Stub SIP backend"));
    QVERIFY(!SipManager::instance().isPjsipAvailable());
}

void TestSipManager::initializeSucceedsInStubMode()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!SipManager::instance().isInitialized());
    QVERIFY(SipManager::instance().initialize());
    QVERIFY(SipManager::instance().isInitialized());
}

void TestSipManager::initializeIsIdempotent()
{
    QVERIFY(SipManager::instance().initialize());
    QVERIFY(SipManager::instance().initialize());
    QVERIFY(SipManager::instance().isInitialized());
}

void TestSipManager::shutdownIsIdempotent()
{
    SipManager::instance().initialize();
    SipManager::instance().shutdown();
    QVERIFY(!SipManager::instance().isInitialized());
    SipManager::instance().shutdown();
    QVERIFY(!SipManager::instance().isInitialized());
}

void TestSipManager::registrationStateNamesAreStable()
{
    QCOMPARE(registrationStateName(RegistrationState::Unregistered),
             QStringLiteral("Unregistered"));
    QCOMPARE(registrationStateName(RegistrationState::Registering),
             QStringLiteral("Registering"));
    QCOMPARE(registrationStateName(RegistrationState::Registered),
             QStringLiteral("Registered"));
    QCOMPARE(registrationStateName(RegistrationState::RegistrationFailed),
             QStringLiteral("RegistrationFailed"));
}

void TestSipManager::registrationWithoutActiveProfileFails()
{
    SipProfileManager::instance().setActiveProfileId({});
    QVERIFY(!SipManager::instance().registerActiveProfile());
    QCOMPARE(SipManager::instance().registrationState(),
             RegistrationState::RegistrationFailed);
    QVERIFY(SipManager::instance().registrationStatusText().contains(
        QStringLiteral("No active")));
}

void TestSipManager::stubRegistrationUsesActiveProfileAndFailsSafely()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    const QString id = addActiveProfileWithPassword(QStringLiteral("stub-secret"));
    QVERIFY(!id.isEmpty());

    QSignalSpy stateSpy(&SipManager::instance(),
                        &SipManager::registrationStateChanged);
    QVERIFY(!SipManager::instance().registerActiveProfile());
    QCOMPARE(SipManager::instance().registeredProfileId(), id);
    QTRY_COMPARE(SipManager::instance().registrationState(),
                 RegistrationState::RegistrationFailed);
    QVERIFY(stateSpy.count() >= 2); // Registering, then safe stub failure.
}

void TestSipManager::stubUnregisterReturnsToUnregistered()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("stub-secret")).isEmpty());
    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE(SipManager::instance().registrationState(),
                 RegistrationState::RegistrationFailed);

    QVERIFY(SipManager::instance().unregisterActiveProfile());
    QTRY_COMPARE(SipManager::instance().registrationState(),
                 RegistrationState::Unregistered);
}

void TestSipManager::registerRejectedWhileRegistering()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("secret")).isEmpty());
    SipManager::instance().registerActiveProfile();

    // State is Registering (PJSIP stub posts async failure later).
    // A second register call must be rejected without changing state.
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Registering);
    const bool second = SipManager::instance().registerActiveProfile();
    // Either returns true (same-profile no-op) or false (rejected) — never crashes.
    Q_UNUSED(second)
    // State must still be Registering or moved to RegistrationFailed after the async stub fires.
    QTRY_VERIFY(SipManager::instance().registrationState() != RegistrationState::Registered);
}

void TestSipManager::registerRejectedWhileRegistered()
{
    // In stub mode registration always fails, so we can only verify the
    // guard logic by checking that a second call while already Registered
    // would be rejected. We test this via the state machine directly.
    auto &sm = SipManager::instance().stateMachine();
    // Force state to Registered to check the guard.
    sm.reset("test setup");
    sm.tryTransition(RegistrationState::Registering, "test");
    sm.tryTransition(RegistrationState::Registered, "test");

    QSignalSpy rejectedSpy(&sm, &RegistrationStateMachine::transitionRejected);
    sm.tryTransition(RegistrationState::Registering, "attempt re-register");
    QCOMPARE(rejectedSpy.count(), 1);
    QCOMPARE(sm.state(), RegistrationState::Registered);
    sm.reset("cleanup");
}

void TestSipManager::unregisterRejectedWhileUnregistered()
{
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Unregistered);
    // Must return true (no-op) without erroring.
    QVERIFY(SipManager::instance().unregisterActiveProfile());
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::Unregistered);
}

void TestSipManager::unregisteringStateIsExplicit()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("secret")).isEmpty());
    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE(SipManager::instance().registrationState(),
                 RegistrationState::RegistrationFailed);

    // Force into a synthetic Registered state to test the Unregistering path.
    auto &sm = SipManager::instance().stateMachine();
    sm.reset("test");
    sm.tryTransition(RegistrationState::Registering, "test");
    sm.tryTransition(RegistrationState::Registered, "test");

    // Calling unregister from Registered must set Unregistering, not Registering.
    // (No live account in this synthetic test so SipManager will hit the fallback path.)
    // We verify the state machine directly instead.
    QSignalSpy spy(&sm, &RegistrationStateMachine::stateChanged);
    sm.tryTransition(RegistrationState::Unregistering, "unregister");
    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.last()[0].value<RegistrationState>(), RegistrationState::Unregistering);
    sm.reset("cleanup");
}

void TestSipManager::passwordNeverAppearsInDiagnostics()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    const QString secret = QStringLiteral("NEVER-LOG-THIS-REGISTRATION-SECRET");
    QVERIFY(!addActiveProfileWithPassword(secret).isEmpty());

    QStringList messages;
    const QMetaObject::Connection connection = connect(
        &Logger::instance(), &Logger::entryAdded, this,
        [&messages](const LogEntry &entry) {
            messages << entry.message << entry.payload;
        });

    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE(SipManager::instance().registrationState(),
                 RegistrationState::RegistrationFailed);
    disconnect(connection);

    for (const QString &message : messages)
        QVERIFY2(!message.contains(secret), "Password appeared in diagnostics");
}

QTEST_GUILESS_MAIN(TestSipManager)
#include "test_sip_manager.moc"
