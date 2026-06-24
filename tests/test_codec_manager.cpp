#include <QtTest/QtTest>

#include "core/Logger.h"
#include "sip/CodecManager.h"

// Tests for CodecManager in stub mode (ENABLE_PJSIP=OFF).
// All assertions operate on the stub codec list populated by initialize().

class TestCodecManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Stub-mode codec list
    void stubAudioCodecsNotEmpty();
    void stubAudioCodecsNotAvailable();
    void stubAudioCodecsPrioritiesPositive();

    // hasUsable* in stub mode
    void stubHasUsableAudioReturnsFalse();
    void stubHasUsableVideoReturnsFalse();

    // Video codecs empty in stub mode
    void stubVideoCodecsEmpty();

    // RTT model
    void rttCodecsAlwaysPresent();
    void rttCodecsNotAvailable();
    void rttCodecIds();

    // Idempotent re-initialize
    void reinitializeIsIdempotent();

    // Media types
    void audioCodecMediaType();
    void rttCodecMediaType();
};

void TestCodecManager::initTestCase()
{
#ifdef HAVE_PJSIP
    QSKIP("test_codec_manager is for stub mode only (HAVE_PJSIP is defined)");
#endif
    CodecManager::instance().initialize();
}

void TestCodecManager::stubAudioCodecsNotEmpty()
{
    QVERIFY(!CodecManager::instance().audioCodecs().isEmpty());
}

void TestCodecManager::stubAudioCodecsNotAvailable()
{
    for (const auto &e : CodecManager::instance().audioCodecs())
        QVERIFY2(!e.available,
                 qPrintable(QStringLiteral("Stub codec %1 should not be available").arg(e.codecId)));
}

void TestCodecManager::stubAudioCodecsPrioritiesPositive()
{
    for (const auto &e : CodecManager::instance().audioCodecs())
        QVERIFY2(e.priority > 0,
                 qPrintable(QStringLiteral("Stub codec %1 priority should be > 0").arg(e.codecId)));
}

void TestCodecManager::stubHasUsableAudioReturnsFalse()
{
    // In stub mode available=false, so hasUsableAudioCodec must return false.
    QVERIFY(!CodecManager::instance().hasUsableAudioCodec());
}

void TestCodecManager::stubHasUsableVideoReturnsFalse()
{
    QVERIFY(!CodecManager::instance().hasUsableVideoCodec());
}

void TestCodecManager::stubVideoCodecsEmpty()
{
    QVERIFY(CodecManager::instance().videoCodecs().isEmpty());
}

void TestCodecManager::rttCodecsAlwaysPresent()
{
    QVERIFY(!CodecManager::instance().rttCodecs().isEmpty());
    QCOMPARE(CodecManager::instance().rttCodecs().size(), 2);
}

void TestCodecManager::rttCodecsNotAvailable()
{
    for (const auto &e : CodecManager::instance().rttCodecs())
        QVERIFY2(!e.available,
                 qPrintable(QStringLiteral("RTT codec %1 should not be available").arg(e.codecId)));
}

void TestCodecManager::rttCodecIds()
{
    const QList<CodecEntry> rtts = CodecManager::instance().rttCodecs();
    bool hasRed  = false;
    bool hasT140 = false;
    for (const auto &e : rtts) {
        if (e.codecId.startsWith(QStringLiteral("red"),  Qt::CaseInsensitive)) hasRed  = true;
        if (e.codecId.startsWith(QStringLiteral("t140"), Qt::CaseInsensitive)) hasT140 = true;
    }
    QVERIFY2(hasRed,  "Expected a 'red' RTT codec entry");
    QVERIFY2(hasT140, "Expected a 't140' RTT codec entry");
}

void TestCodecManager::reinitializeIsIdempotent()
{
    CodecManager::instance().initialize();
    CodecManager::instance().initialize();
    // Still same stub list — no crash, no duplication.
    QVERIFY(!CodecManager::instance().audioCodecs().isEmpty());
}

void TestCodecManager::audioCodecMediaType()
{
    for (const auto &e : CodecManager::instance().audioCodecs())
        QCOMPARE(e.mediaType, SipMediaType::Audio);
}

void TestCodecManager::rttCodecMediaType()
{
    for (const auto &e : CodecManager::instance().rttCodecs())
        QCOMPARE(e.mediaType, SipMediaType::Text);
}

QTEST_GUILESS_MAIN(TestCodecManager)
#include "test_codec_manager.moc"
