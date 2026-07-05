#include <QtTest/QtTest>

#include "core/CodecInfo.h"

// Tests for the structured codec models used by the Diagnostics Center and
// the Diagnostics Bundle: JSON round-trip, summary formatting (including
// which parts are omitted when unknown) and invalid/default codecs.

class TestCodecInfo : public QObject
{
    Q_OBJECT

private slots:
    void audioJsonRoundTrip();
    void videoJsonRoundTrip();
    void audioSummaryFormatting();
    void videoSummaryFormatting();
    void invalidCodec();
};

void TestCodecInfo::audioJsonRoundTrip()
{
    AudioCodecInfo c;
    c.name = QStringLiteral("PCMA");
    c.payloadType = 8;
    c.clockRate = 8000;
    c.channels = 1;
    c.ptime = 20;
    c.bitrate = 64000;
    c.negotiated = true;

    const AudioCodecInfo r = AudioCodecInfo::fromJson(c.toJson());
    QCOMPARE(r.name, c.name);
    QCOMPARE(r.payloadType, c.payloadType);
    QCOMPARE(r.clockRate, c.clockRate);
    QCOMPARE(r.channels, c.channels);
    QCOMPARE(r.ptime, c.ptime);
    QCOMPARE(r.bitrate, c.bitrate);
    QCOMPARE(r.negotiated, c.negotiated);
    QVERIFY(r.isValid());

    // Round-tripping a default (invalid) codec must not fabricate values.
    const AudioCodecInfo empty = AudioCodecInfo::fromJson(AudioCodecInfo().toJson());
    QVERIFY(!empty.isValid());
    QCOMPARE(empty.payloadType, -1);
    QCOMPARE(empty.clockRate, 0);
}

void TestCodecInfo::videoJsonRoundTrip()
{
    VideoCodecInfo c;
    c.name = QStringLiteral("H264");
    c.payloadType = 97;
    c.width = 1280;
    c.height = 720;
    c.fps = 30;
    c.bitrate = 512000;
    c.negotiated = true;

    const VideoCodecInfo r = VideoCodecInfo::fromJson(c.toJson());
    QCOMPARE(r.name, c.name);
    QCOMPARE(r.payloadType, c.payloadType);
    QCOMPARE(r.width, c.width);
    QCOMPARE(r.height, c.height);
    QCOMPARE(r.fps, c.fps);
    QCOMPARE(r.bitrate, c.bitrate);
    QCOMPARE(r.negotiated, c.negotiated);
    QVERIFY(r.isValid());

    const VideoCodecInfo empty = VideoCodecInfo::fromJson(VideoCodecInfo().toJson());
    QVERIFY(!empty.isValid());
    QCOMPARE(empty.payloadType, -1);
    QCOMPARE(empty.width, 0);
}

void TestCodecInfo::audioSummaryFormatting()
{
    AudioCodecInfo c;
    c.name = QStringLiteral("PCMA");
    c.payloadType = 8;
    c.clockRate = 8000;
    c.channels = 1;
    c.ptime = 20;
    QCOMPARE(c.summaryString(), QStringLiteral("PCMA • 8000 Hz • PT=8 • Mono • 20 ms"));

    // Stereo and bitrate variants.
    AudioCodecInfo opus;
    opus.name = QStringLiteral("opus");
    opus.payloadType = 111;
    opus.clockRate = 48000;
    opus.channels = 2;
    opus.ptime = 20;
    opus.bitrate = 32000;
    QCOMPARE(opus.summaryString(),
             QStringLiteral("opus • 48000 Hz • PT=111 • Stereo • 20 ms • 32 kbps"));

    // Unknown channels / ptime are omitted, never invented.
    AudioCodecInfo bare;
    bare.name = QStringLiteral("PCMU");
    bare.payloadType = 0;
    bare.clockRate = 8000;
    QCOMPARE(bare.summaryString(), QStringLiteral("PCMU • 8000 Hz • PT=0"));
}

void TestCodecInfo::videoSummaryFormatting()
{
    VideoCodecInfo c;
    c.name = QStringLiteral("H264");
    c.payloadType = 97;
    c.width = 1280;
    c.height = 720;
    c.fps = 30;
    c.bitrate = 512000;
    QCOMPARE(c.summaryString(),
             QStringLiteral("H264 • 1280x720 • 30 fps • PT=97 • 512 kbps"));

    // Only the codec name is known.
    VideoCodecInfo bare;
    bare.name = QStringLiteral("VP8");
    QCOMPARE(bare.summaryString(), QStringLiteral("VP8"));
}

void TestCodecInfo::invalidCodec()
{
    const AudioCodecInfo audio;
    QVERIFY(!audio.isValid());
    QCOMPARE(audio.summaryString(), QStringLiteral("N/A"));

    // A name alone is not enough for audio: clock rate and payload type are
    // required for the codec to be meaningful.
    AudioCodecInfo nameOnly;
    nameOnly.name = QStringLiteral("PCMA");
    QVERIFY(!nameOnly.isValid());
    QCOMPARE(nameOnly.summaryString(), QStringLiteral("N/A"));

    const VideoCodecInfo video;
    QVERIFY(!video.isValid());
    QCOMPARE(video.summaryString(), QStringLiteral("N/A"));
}

QTEST_GUILESS_MAIN(TestCodecInfo)
#include "test_codec_info.moc"
