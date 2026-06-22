#include <QtTest/QtTest>
#include <QCoreApplication>

#include "sip/SipManager.h"

// These tests always run against the stub backend (PJSIP not required).
// They verify: stub mode behavior, initialize/shutdown idempotency, backendName(),
// and that the manager does not crash or throw without PJSIP installed.

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
};

void TestSipManager::init()
{
    // Reset manager state between tests via shutdown so each test starts clean.
    SipManager::instance().shutdown();
}

void TestSipManager::cleanup()
{
    SipManager::instance().shutdown();
}

// ---------------------------------------------------------------------------
// 1. stubBackendNameIsCorrect
//    Without HAVE_PJSIP, backendName() must return the stub name.
//    isPjsipAvailable() must be false.
// ---------------------------------------------------------------------------
void TestSipManager::stubBackendNameIsCorrect()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QCOMPARE(SipManager::instance().backendName(),
             QStringLiteral("Stub SIP backend"));
    QVERIFY(!SipManager::instance().isPjsipAvailable());
}

// ---------------------------------------------------------------------------
// 2. initializeSucceedsInStubMode
//    initialize() must return true and set isInitialized() even without PJSIP.
// ---------------------------------------------------------------------------
void TestSipManager::initializeSucceedsInStubMode()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    QVERIFY(!SipManager::instance().isInitialized());
    const bool ok = SipManager::instance().initialize();
    QVERIFY(ok);
    QVERIFY(SipManager::instance().isInitialized());
}

// ---------------------------------------------------------------------------
// 3. initializeIsIdempotent
//    Calling initialize() twice must return true both times without side effects.
// ---------------------------------------------------------------------------
void TestSipManager::initializeIsIdempotent()
{
    const bool first  = SipManager::instance().initialize();
    const bool second = SipManager::instance().initialize();
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(SipManager::instance().isInitialized());
}

// ---------------------------------------------------------------------------
// 4. shutdownIsIdempotent
//    shutdown() called multiple times must not crash.
// ---------------------------------------------------------------------------
void TestSipManager::shutdownIsIdempotent()
{
    SipManager::instance().initialize();
    SipManager::instance().shutdown();
    QVERIFY(!SipManager::instance().isInitialized());
    // Second shutdown must not crash
    SipManager::instance().shutdown();
    QVERIFY(!SipManager::instance().isInitialized());
}

QTEST_GUILESS_MAIN(TestSipManager)
#include "test_sip_manager.moc"
