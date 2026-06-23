#include <QtTest/QtTest>

#include "core/Logger.h"
#include "media/VideoMediaManager.h"
#include "sip/SipCall.h"
#include "sip/CallStateMachine.h"

// Tests for VideoMediaManager in stub mode (ENABLE_PJSIP=OFF).
// All tests drive a standalone SipCall directly via attachCall(); no SipManager
// required, avoiding the singleton's registration machinery.

class TestVideoMedia : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Video media lifecycle
    void attachVideoOnActive();
    void detachVideoOnHangup();

    // Video mute
    void videoMuteUnmute();

    // Camera switch persistence (stub — no PJSIP device wiring)
    void cameraSwitchDuringCall();

    // Edge cases
    void noCrashWithoutCall();
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

void TestVideoMedia::init()
{
#ifdef HAVE_PJSIP
    QSKIP("Test is for stub mode only (HAVE_PJSIP is defined)");
#endif
    // Detach any stale call left from a previous test.
    VideoMediaManager::instance().detachCall();
}

void TestVideoMedia::cleanup()
{
    VideoMediaManager::instance().detachCall();
    // Reset video-mute state between tests.
    VideoMediaManager::instance().setVideoMuted(false);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void TestVideoMedia::attachVideoOnActive()
{
    VideoMediaManager &mgr = VideoMediaManager::instance();

    SipCall call;
    QSignalSpy connectedSpy(&mgr, &VideoMediaManager::videoMediaConnected);

    mgr.attachCall(&call);
    driveToActive(call);

    // In stub mode videoMediaConnected is emitted synchronously when the SM
    // reaches Active alongside audioMediaConnected.
    QCOMPARE(connectedSpy.count(), 1);
    QVERIFY(mgr.isVideoActive());
    QVERIFY(mgr.isLocalVideoAvailable());
    QVERIFY(mgr.isRemoteVideoAvailable());
}

void TestVideoMedia::detachVideoOnHangup()
{
    VideoMediaManager &mgr = VideoMediaManager::instance();

    SipCall call;
    QSignalSpy disconnectedSpy(&mgr, &VideoMediaManager::videoMediaDisconnected);

    mgr.attachCall(&call);
    driveToActive(call);
    QVERIFY(mgr.isVideoActive());

    // Hang up: Active → Disconnecting (sync) → Idle (queued).
    call.hangup();
    QTest::qWait(50); // let the queued Idle transition fire

    QCOMPARE(disconnectedSpy.count(), 1);
    QVERIFY(!mgr.isVideoActive());
    QVERIFY(!mgr.isLocalVideoAvailable());
    QVERIFY(!mgr.isRemoteVideoAvailable());
}

void TestVideoMedia::videoMuteUnmute()
{
    VideoMediaManager &mgr = VideoMediaManager::instance();

    SipCall call;
    QSignalSpy muteSpy(&mgr, &VideoMediaManager::videoMutedChanged);

    mgr.attachCall(&call);
    driveToActive(call);

    // Mute video
    mgr.setVideoMuted(true);
    QVERIFY(mgr.isVideoMuted());
    QVERIFY(call.isVideoMuted());
    QCOMPARE(muteSpy.count(), 1);
    QCOMPARE(muteSpy.at(0).at(0).toBool(), true);

    // Unmute
    mgr.setVideoMuted(false);
    QVERIFY(!mgr.isVideoMuted());
    QVERIFY(!call.isVideoMuted());
    QCOMPARE(muteSpy.count(), 2);
    QCOMPARE(muteSpy.at(1).at(0).toBool(), false);

    // Idempotent — no extra signal when setting same state
    mgr.setVideoMuted(false);
    QCOMPARE(muteSpy.count(), 2);
}

void TestVideoMedia::cameraSwitchDuringCall()
{
    VideoMediaManager &mgr = VideoMediaManager::instance();

    SipCall call;
    mgr.attachCall(&call);
    driveToActive(call);
    QVERIFY(mgr.isVideoActive());

    // In stub mode there are no real cameras; setCamera should not crash
    // when the device ID is not found — it should just log a warning.
    mgr.setCamera(QStringLiteral("nonexistent-camera-id"));

    // State is still active (no side effects from failed camera switch).
    QVERIFY(mgr.isVideoActive());
}

void TestVideoMedia::noCrashWithoutCall()
{
    // VideoMediaManager can be queried before any call is attached.
    VideoMediaManager &mgr = VideoMediaManager::instance();

    QVERIFY(!mgr.isVideoActive());
    QVERIFY(!mgr.isLocalVideoAvailable());
    QVERIFY(!mgr.isRemoteVideoAvailable());
    QVERIFY(!mgr.isVideoMuted());

    // setVideoMuted before attaching a call stores the flag and does not crash.
    mgr.setVideoMuted(true);
    QVERIFY(mgr.isVideoMuted());
    mgr.setVideoMuted(false);

    // detachCall() on already-detached manager is safe (double-detach).
    mgr.detachCall();
    mgr.detachCall();
}

void TestVideoMedia::activeCallCleanup()
{
    VideoMediaManager &mgr = VideoMediaManager::instance();

    SipCall call;
    QSignalSpy disconnectedSpy(&mgr, &VideoMediaManager::videoMediaDisconnected);

    mgr.attachCall(&call);
    driveToActive(call);
    QVERIFY(mgr.isVideoActive());

    // Simulate abrupt call removal by detaching explicitly mid-call.
    mgr.detachCall();

    QCOMPARE(disconnectedSpy.count(), 1);
    QVERIFY(!mgr.isVideoActive());
    QVERIFY(!mgr.isLocalVideoAvailable());
    QVERIFY(!mgr.isRemoteVideoAvailable());
}

QTEST_GUILESS_MAIN(TestVideoMedia)
#include "test_video_media.moc"
