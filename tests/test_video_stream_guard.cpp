#include <QtTest/QtTest>

#include "sip/VideoStreamGuard.h"

using namespace VideoStreamGuard;

class TestVideoStreamGuard : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUnsafeState_data();
    void rejectsUnsafeState();
    void acceptsOnlyActiveTransmitVideo();
    void operationIsNeverInvokedForRejectedDecision();
    void repeatedDecisionsAreStable();
};

static Snapshot validSnapshot()
{
    Snapshot snapshot;
    snapshot.callObjectExists = true;
    snapshot.callId = 7;
    snapshot.callState = CallState::Confirmed;
    snapshot.currentVideoStreamIndex = 1;
    snapshot.streamExists = true;
    snapshot.media = {
        {0, MediaType::Audio, MediaStatus::Active, true},
        {1, MediaType::Video, MediaStatus::Active, true}
    };
    return snapshot;
}

void TestVideoStreamGuard::rejectsUnsafeState_data()
{
    QTest::addColumn<int>("scenario");
    QTest::newRow("no-call") << 0;
    QTest::newRow("invalid-call-id") << 1;
    QTest::newRow("null-call") << 2;
    QTest::newRow("disconnected") << 3;
    QTest::newRow("teardown") << 4;
    QTest::newRow("stream-not-created") << 5;
    QTest::newRow("audio-only") << 6;
    QTest::newRow("inactive-video") << 7;
    QTest::newRow("video-error") << 8;
    QTest::newRow("invalid-media-index") << 9;
    QTest::newRow("stream-index-mismatch") << 10;
    QTest::newRow("receive-only-video") << 11;
    QTest::newRow("local-hold") << 12;
    QTest::newRow("remote-hold") << 13;
    QTest::newRow("null-internal-stream") << 14;
}

void TestVideoStreamGuard::rejectsUnsafeState()
{
    QFETCH(int, scenario);
    Snapshot snapshot = validSnapshot();
    switch (scenario) {
    case 0: snapshot.callObjectExists = false; break;
    case 1: snapshot.callId = -1; break;
    case 2: snapshot.callState = CallState::Null; break;
    case 3: snapshot.callState = CallState::Disconnected; break;
    case 4: snapshot.teardownInProgress = true; break;
    case 5: snapshot.currentVideoStreamIndex = -1; break;
    case 6: snapshot.media.resize(1); break;
    case 7: snapshot.media[1].status = MediaStatus::None; break;
    case 8: snapshot.media[1].status = MediaStatus::Error; break;
    case 9:
        snapshot.currentVideoStreamIndex = -2;
        snapshot.media[1].index = -2;
        break;
    case 10: snapshot.currentVideoStreamIndex = 2; break;
    case 11: snapshot.media[1].canTransmit = false; break;
    case 12: snapshot.media[1].status = MediaStatus::LocalHold; break;
    case 13: snapshot.media[1].status = MediaStatus::RemoteHold; break;
    case 14: snapshot.streamExists = false; break;
    }

    const Decision decision = evaluate(snapshot);
    QVERIFY(!decision.mayOperate);
    QCOMPARE(decision.mediaIndex, -1);
    QVERIFY(decision.reason != nullptr);
}

void TestVideoStreamGuard::acceptsOnlyActiveTransmitVideo()
{
    const Decision decision = evaluate(validSnapshot());
    QVERIFY(decision.mayOperate);
    QCOMPARE(decision.mediaIndex, 1);
}

void TestVideoStreamGuard::operationIsNeverInvokedForRejectedDecision()
{
    Snapshot snapshot = validSnapshot();
    snapshot.media[1].status = MediaStatus::Error;
    int operationCount = 0;
    const Decision decision = evaluate(snapshot);
    if (decision.mayOperate)
        ++operationCount; // fake vidSetStream wrapper
    QCOMPARE(operationCount, 0);
}

void TestVideoStreamGuard::repeatedDecisionsAreStable()
{
    const Snapshot snapshot = validSnapshot();
    const Decision cameraOff1 = evaluate(snapshot);
    const Decision cameraOff2 = evaluate(snapshot);
    const Decision cameraOn1 = evaluate(snapshot);
    const Decision cameraOn2 = evaluate(snapshot);
    QVERIFY(cameraOff1.mayOperate);
    QCOMPARE(cameraOff1.mediaIndex, cameraOff2.mediaIndex);
    QCOMPARE(cameraOn1.mediaIndex, cameraOn2.mediaIndex);
}

QTEST_GUILESS_MAIN(TestVideoStreamGuard)
#include "test_video_stream_guard.moc"
