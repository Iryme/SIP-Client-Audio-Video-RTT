#include <QtTest/QtTest>

#include "core/Logger.h"
#include "security/CredentialStore.h"
#include "security/MemoryCredentialBackend.h"
#include "sip/RegistrationRetryPolicy.h"
#include "sip/SipAccount.h"
#include "sip/SipManager.h"
#include "sip/SipProfileManager.h"

class TestRegistrationRetry : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // RegistrationRetryPolicy unit tests
    void policyRetryableForStatusZero();
    void policyRetryableFor408();
    void policyRetryableFor5xx();
    void policyNotRetryableFor401();
    void policyNotRetryableFor403();
    void policyNotRetryableFor404();
    void policyDelayGrowsExponentially();
    void policyDelayIsCappedAtMax();

    // SipManager integration tests (stub mode only)
    void retryScheduledOnTransientFailure();
    void retryNotScheduledWhenMaxAttemptsZero();
    void retryAttemptCounterIncrements();
    void unregisterStopsRetryAndResetsCounter();

private:
    QString addActiveProfileWithPassword(const QString &password);
    QString m_createdProfileId;
    QString m_originalActiveProfileId;
};

void TestRegistrationRetry::init()
{
    SipManager::instance().shutdown();
    m_originalActiveProfileId = SipProfileManager::instance().activeProfileId();
    CredentialStore::instance().setBackend(std::make_unique<MemoryCredentialBackend>());

    // Default policy for integration tests: long delay so the retry timer never
    // fires during a test unless the test explicitly controls it.
    RegistrationRetryPolicy noAutoFire;
    noAutoFire.maxAttempts  = 3;
    noAutoFire.initialDelayMs = 30000;
    noAutoFire.maxDelayMs   = 60000;
    SipManager::instance().setRetryPolicy(noAutoFire);
}

void TestRegistrationRetry::cleanup()
{
    SipManager::instance().shutdown();
    if (!m_createdProfileId.isEmpty()) {
        SipProfileManager::instance().remove(m_createdProfileId);
        m_createdProfileId.clear();
    }
    SipProfileManager::instance().setActiveProfileId(m_originalActiveProfileId);
}

QString TestRegistrationRetry::addActiveProfileWithPassword(const QString &password)
{
    SipProfile profile = SipProfile::createNew();
    profile.displayName  = QStringLiteral("Retry Test");
    profile.sipUsername  = QStringLiteral("retry-test");
    profile.sipDomain    = QStringLiteral("example.invalid");
    profile.registrar    = QStringLiteral("example.invalid");
    profile.authUsername = QStringLiteral("retry-auth");

    m_createdProfileId = SipProfileManager::instance().add(profile);
    if (m_createdProfileId.isEmpty())
        return {};
    SipProfileManager::instance().setActiveProfileId(m_createdProfileId);
    if (!SipProfileManager::instance().setProfilePassword(m_createdProfileId, password))
        return {};
    return m_createdProfileId;
}

// --- RegistrationRetryPolicy unit tests ---

void TestRegistrationRetry::policyRetryableForStatusZero()
{
    QVERIFY(RegistrationRetryPolicy::isRetryable(0));
}

void TestRegistrationRetry::policyRetryableFor408()
{
    QVERIFY(RegistrationRetryPolicy::isRetryable(408));
}

void TestRegistrationRetry::policyRetryableFor5xx()
{
    QVERIFY(RegistrationRetryPolicy::isRetryable(500));
    QVERIFY(RegistrationRetryPolicy::isRetryable(503));
    QVERIFY(RegistrationRetryPolicy::isRetryable(504));
    QVERIFY(RegistrationRetryPolicy::isRetryable(599));
}

void TestRegistrationRetry::policyNotRetryableFor401()
{
    QVERIFY(!RegistrationRetryPolicy::isRetryable(401));
}

void TestRegistrationRetry::policyNotRetryableFor403()
{
    QVERIFY(!RegistrationRetryPolicy::isRetryable(403));
}

void TestRegistrationRetry::policyNotRetryableFor404()
{
    QVERIFY(!RegistrationRetryPolicy::isRetryable(404));
}

void TestRegistrationRetry::policyDelayGrowsExponentially()
{
    RegistrationRetryPolicy p;
    p.initialDelayMs = 1000;
    p.multiplier     = 2.0;
    p.maxDelayMs     = 120000;

    QCOMPARE(p.delayForAttempt(1), 1000);
    QCOMPARE(p.delayForAttempt(2), 2000);
    QCOMPARE(p.delayForAttempt(3), 4000);
    QCOMPARE(p.delayForAttempt(4), 8000);
}

void TestRegistrationRetry::policyDelayIsCappedAtMax()
{
    RegistrationRetryPolicy p;
    p.initialDelayMs = 2000;
    p.multiplier     = 2.0;
    p.maxDelayMs     = 5000;

    QVERIFY(p.delayForAttempt(10) <= 5000);
    QCOMPARE(p.delayForAttempt(10), 5000);
}

// --- SipManager integration tests (stub mode only) ---

void TestRegistrationRetry::retryScheduledOnTransientFailure()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("secret")).isEmpty());

    RegistrationRetryPolicy policy;
    policy.maxAttempts    = 2;
    policy.initialDelayMs = 30000;  // long — we just want the signal, not the retry
    policy.maxDelayMs     = 60000;
    SipManager::instance().setRetryPolicy(policy);

    QSignalSpy retrySpy(&SipManager::instance(), &SipManager::retryScheduled);
    SipManager::instance().registerActiveProfile();

    QTRY_COMPARE(retrySpy.count(), 1);
    QCOMPARE(retrySpy[0][0].toInt(), 1);      // attempt number
    QCOMPARE(retrySpy[0][1].toInt(), 30000);  // delay
    QCOMPARE(SipManager::instance().retryAttempt(), 1);
}

void TestRegistrationRetry::retryNotScheduledWhenMaxAttemptsZero()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("secret")).isEmpty());

    RegistrationRetryPolicy policy;
    policy.maxAttempts = 0;
    SipManager::instance().setRetryPolicy(policy);

    QSignalSpy retrySpy(&SipManager::instance(), &SipManager::retryScheduled);
    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE(SipManager::instance().registrationState(), RegistrationState::RegistrationFailed);

    QTest::qWait(50);
    QCOMPARE(retrySpy.count(), 0);
    QCOMPARE(SipManager::instance().retryAttempt(), 0);
}

void TestRegistrationRetry::retryAttemptCounterIncrements()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("secret")).isEmpty());

    RegistrationRetryPolicy policy;
    policy.maxAttempts    = 3;
    policy.initialDelayMs = 0;  // fire immediately so we can observe multiple retries
    policy.maxDelayMs     = 0;
    SipManager::instance().setRetryPolicy(policy);

    QSignalSpy retrySpy(&SipManager::instance(), &SipManager::retryScheduled);

    SipManager::instance().registerActiveProfile();

    // After maxAttempts retries the signal must have fired exactly maxAttempts times.
    QTRY_COMPARE_WITH_TIMEOUT(retrySpy.count(), 3, 5000);
    QCOMPARE(SipManager::instance().retryAttempt(), 0);  // reset after exhaustion
    QCOMPARE(SipManager::instance().registrationState(), RegistrationState::RegistrationFailed);
}

void TestRegistrationRetry::unregisterStopsRetryAndResetsCounter()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!addActiveProfileWithPassword(QStringLiteral("secret")).isEmpty());

    RegistrationRetryPolicy policy;
    policy.maxAttempts    = 3;
    policy.initialDelayMs = 30000;  // long — retry must not fire during this test
    policy.maxDelayMs     = 60000;
    SipManager::instance().setRetryPolicy(policy);

    SipManager::instance().registerActiveProfile();
    QTRY_COMPARE(SipManager::instance().registrationState(), RegistrationState::RegistrationFailed);
    QCOMPARE(SipManager::instance().retryAttempt(), 1);

    // Unregister must cancel the pending retry and reset the counter.
    SipManager::instance().unregisterActiveProfile();
    QCOMPARE(SipManager::instance().retryAttempt(), 0);

    QTRY_COMPARE(SipManager::instance().registrationState(), RegistrationState::Unregistered);
}

QTEST_GUILESS_MAIN(TestRegistrationRetry)
#include "test_registration_retry.moc"
