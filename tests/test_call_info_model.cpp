// Task W113 — Call Workspace
//
// Unit tests for CallInfoModel: the single source of truth for the active
// call's display state. CallInfoModel is deliberately fed via setters (no
// SipManager/pjsua2 dependency), so these tests exercise it with synthetic
// inputs, QTEST_GUILESS_MAIN (no QApplication needed).

#include <QtTest/QtTest>

#include "gui/panels/call/CallInfoModel.h"

class TestCallInfoModel : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreEmpty()
    {
        CallInfoModel model;
        QCOMPARE(model.state(), CallState::Idle);
        QVERIFY(model.remoteUri().isEmpty());
        QVERIFY(model.displayName().isEmpty());
        QCOMPARE(model.durationSeconds(), 0);
        QVERIFY(!model.muted());
        QVERIFY(!model.held());
        QVERIFY(!model.negotiatedAudio().isValid());
        QVERIFY(!model.negotiatedVideo().isValid());
        QVERIFY(!model.rtpStats().available);
    }

    void settersUpdateFieldsAndEmitSignal()
    {
        CallInfoModel model;
        QSignalSpy spy(&model, &CallInfoModel::callInfoChanged);

        model.setState(CallState::Active, QStringLiteral("Active"));
        model.setRemoteUri(QStringLiteral("sip:alice@example.com"));
        model.setDisplayName(QStringLiteral("Alice"));
        model.setMuted(true);
        model.setHeld(true);
        model.setDurationSeconds(42);

        QCOMPARE(model.state(), CallState::Active);
        QCOMPARE(model.remoteUri(), QStringLiteral("sip:alice@example.com"));
        QCOMPARE(model.displayName(), QStringLiteral("Alice"));
        QVERIFY(model.muted());
        QVERIFY(model.held());
        QCOMPARE(model.durationSeconds(), 42);
        QCOMPARE(spy.count(), 6);
    }

    void selectedMediaIsIndependentFromNegotiated()
    {
        // "Selected" (what was requested at call-launch) and "negotiated"
        // (what the SDP exchange actually produced) must never be conflated
        // — a call can request video+RTT but negotiate audio-only.
        CallInfoModel model;
        model.setSelectedMedia(CallMediaOptions::fromType(CallType::AudioVideoRtt));
        AudioCodecInfo negotiatedAudio;
        negotiatedAudio.name = QStringLiteral("PCMA");
        negotiatedAudio.payloadType = 8;
        negotiatedAudio.clockRate = 8000;
        model.setNegotiatedAudio(negotiatedAudio);

        QCOMPARE(model.selectedMedia().type, CallType::AudioVideoRtt);
        QVERIFY(model.selectedMedia().enableVideo);
        QVERIFY(model.selectedMedia().enableRtt);
        QVERIFY(model.negotiatedAudio().isValid());
        QVERIFY(!model.negotiatedVideo().isValid());
    }

    void packetLossAndVideoDropsStaySeparate()
    {
        // Regression guard for the bug found in W112/W113 research: RTP-stats
        // packet loss (%) and the video pipeline's own frame-drop count (raw
        // count) must never be written into the same field.
        CallInfoModel model;
        RtpStatsSnapshot stats;
        stats.available = true;
        stats.packetLossAvailable = true;
        stats.packetLossPercent = 2.5;
        model.setRtpStats(stats);
        model.setVideoStats(29.5f, 7);

        QCOMPARE(model.rtpStats().packetLossPercent, 2.5);
        QCOMPARE(model.videoDropsPerSecond(), 7);
        QCOMPARE(model.videoFps(), 29.5f);
    }

    void resetClearsEveryField()
    {
        CallInfoModel model;
        model.setState(CallState::Active, QStringLiteral("Active"));
        model.setRemoteUri(QStringLiteral("sip:bob@example.com"));
        model.setDisplayName(QStringLiteral("Bob"));
        model.setMuted(true);
        model.setHeld(true);
        model.setVideoConnected(true);
        model.setRttConnected(true);
        model.setSelectedMedia(CallMediaOptions::fromType(CallType::AudioVideoRtt));
        AudioCodecInfo audio;
        audio.name = QStringLiteral("opus");
        audio.payloadType = 111;
        audio.clockRate = 48000;
        model.setNegotiatedAudio(audio);
        RtpStatsSnapshot stats;
        stats.available = true;
        model.setRtpStats(stats);
        model.setDeviceNames(QStringLiteral("Mic"), QStringLiteral("Speaker"), QStringLiteral("Camera"));

        model.reset();

        QCOMPARE(model.state(), CallState::Idle);
        QVERIFY(model.remoteUri().isEmpty());
        QVERIFY(model.displayName().isEmpty());
        QVERIFY(!model.muted());
        QVERIFY(!model.held());
        QVERIFY(!model.videoConnected());
        QVERIFY(!model.rttConnected());
        QCOMPARE(model.selectedMedia().type, CallType::AudioOnly);
        QVERIFY(!model.negotiatedAudio().isValid());
        QVERIFY(!model.rtpStats().available);
        QVERIFY(model.micDeviceName().isEmpty());
        QVERIFY(model.speakerDeviceName().isEmpty());
        QVERIFY(model.cameraDeviceName().isEmpty());
    }

    void resetIsolatesSuccessiveCalls()
    {
        // Concretely what "call isolation" means at the single-active-call
        // level this task delivers: nothing from a finished call may leak
        // into the next one's display.
        CallInfoModel model;
        model.setRemoteUri(QStringLiteral("sip:alice@example.com"));
        model.setVideoConnected(true);
        model.reset();

        model.setRemoteUri(QStringLiteral("sip:carol@example.com"));
        QCOMPARE(model.remoteUri(), QStringLiteral("sip:carol@example.com"));
        QVERIFY(!model.videoConnected());
    }
};

QTEST_GUILESS_MAIN(TestCallInfoModel)
#include "test_call_info_model.moc"
