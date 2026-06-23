#include <QtTest/QtTest>

#include "core/Logger.h"
#include "media/AudioMediaManager.h"
#include "sip/SipCall.h"
#include "sip/CallStateMachine.h"

// Tests for AudioMediaManager in stub mode (ENABLE_PJSIP=OFF).
// All tests drive a standalone SipCall directly via attachCall(); no SipManager
// required, avoiding the singleton's registration machinery.

class TestAudioMedia : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Media lifecycle
    void connectMediaOnActive();
    void disconnectMediaOnHangup();

    // Mute
    void muteUnmute();

    // Device switch persistence (stub — no PJSIP device wiring)
    void deviceSwitchDuringCall();

    // Edge cases
    void noCrashWithoutDevices();
    void activeCallCleanup();
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void driveToActive(SipCall &call)
{
    // Incoming call flow: IncomingRinging → Connecting → Active (stub).
    call.stateMachine().tryTransition(CallState::IncomingRinging,
                                      QStringLiteral("Incoming"));
    call.stateMachine().tryTransition(CallState::Connecting,
                                      QStringLiteral("Answering"));
    call.stateMachine().tryTransition(CallState::Active,
                                      QStringLiteral("Connected"), 200);
}

// ---------------------------------------------------------------------------
// init / cleanup
// ---------------------------------------------------------------------------

void TestAudioMedia::init()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    // Detach any stale call left from a previous test.
    AudioMediaManager::instance().detachCall();
}

void TestAudioMedia::cleanup()
{
    AudioMediaManager::instance().detachCall();
    // Reset mute state between tests.
    AudioMediaManager::instance().setMuted(false);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void TestAudioMedia::connectMediaOnActive()
{
    AudioMediaManager &mgr = AudioMediaManager::instance();

    SipCall call;
    QSignalSpy connectedSpy(&mgr, &AudioMediaManager::mediaConnected);

    mgr.attachCall(&call);
    driveToActive(call);

    // In stub mode, audioMediaConnected is emitted synchronously when the SM
    // reaches Active (see SipCall::onStateMachineStateChanged).
    QCOMPARE(connectedSpy.count(), 1);
    QVERIFY(mgr.isMediaActive());
}

void TestAudioMedia::disconnectMediaOnHangup()
{
    AudioMediaManager &mgr = AudioMediaManager::instance();

    SipCall call;
    QSignalSpy disconnectedSpy(&mgr, &AudioMediaManager::mediaDisconnected);

    mgr.attachCall(&call);
    driveToActive(call);
    QVERIFY(mgr.isMediaActive());

    // Hang up: Active → Disconnecting (sync) → Idle (queued).
    call.hangup();
    QTest::qWait(50); // let the queued Idle transition fire

    QCOMPARE(disconnectedSpy.count(), 1);
    QVERIFY(!mgr.isMediaActive());
    QCOMPARE(mgr.inputLevel(), 0);
    QCOMPARE(mgr.outputLevel(), 0);
}

void TestAudioMedia::muteUnmute()
{
    AudioMediaManager &mgr = AudioMediaManager::instance();

    SipCall call;
    QSignalSpy muteSpy(&mgr, &AudioMediaManager::mutedChanged);

    mgr.attachCall(&call);
    driveToActive(call);

    // Mute
    mgr.setMuted(true);
    QVERIFY(mgr.isMuted());
    QVERIFY(call.isMuted());
    QCOMPARE(muteSpy.count(), 1);
    QCOMPARE(muteSpy.at(0).at(0).toBool(), true);

    // Unmute
    mgr.setMuted(false);
    QVERIFY(!mgr.isMuted());
    QVERIFY(!call.isMuted());
    QCOMPARE(muteSpy.count(), 2);
    QCOMPARE(muteSpy.at(1).at(0).toBool(), false);

    // Idempotent — no extra signal when setting same state
    mgr.setMuted(false);
    QCOMPARE(muteSpy.count(), 2);
}

void TestAudioMedia::deviceSwitchDuringCall()
{
    AudioMediaManager &mgr = AudioMediaManager::instance();

    SipCall call;
    mgr.attachCall(&call);
    driveToActive(call);
    QVERIFY(mgr.isMediaActive());

    // In stub mode there are no real devices; setMicrophone/setSpeaker should
    // not crash when the device ID is not found — they just warn.
    mgr.setMicrophone(QStringLiteral("nonexistent-device-id"));
    mgr.setSpeaker   (QStringLiteral("nonexistent-device-id"));

    // Verify state is still active (no side effects from failed device switch).
    QVERIFY(mgr.isMediaActive());
}

void TestAudioMedia::noCrashWithoutDevices()
{
    // AudioMediaManager can be queried before any call is attached.
    AudioMediaManager &mgr = AudioMediaManager::instance();

    QVERIFY(!mgr.isMediaActive());
    QCOMPARE(mgr.inputLevel(),  0);
    QCOMPARE(mgr.outputLevel(), 0);
    QVERIFY(!mgr.isMuted());

    // setMuted before attaching a call stores the flag and does not crash.
    mgr.setMuted(true);
    QVERIFY(mgr.isMuted());
    mgr.setMuted(false);

    // detachCall() on already-detached manager is safe.
    mgr.detachCall();
}

void TestAudioMedia::activeCallCleanup()
{
    AudioMediaManager &mgr = AudioMediaManager::instance();

    SipCall call;
    QSignalSpy disconnectedSpy(&mgr, &AudioMediaManager::mediaDisconnected);

    mgr.attachCall(&call);
    driveToActive(call);
    QVERIFY(mgr.isMediaActive());

    // Simulate abrupt call object going away by detaching explicitly.
    mgr.detachCall();

    QCOMPARE(disconnectedSpy.count(), 1);
    QVERIFY(!mgr.isMediaActive());
    QCOMPARE(mgr.inputLevel(),  0);
    QCOMPARE(mgr.outputLevel(), 0);
}

QTEST_GUILESS_MAIN(TestAudioMedia)
#include "test_audio_media.moc"
