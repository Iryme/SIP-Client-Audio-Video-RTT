#include <QtTest/QtTest>

#include "etsi/UnconfirmedLmpeCodec.h"

// Task W103 — guards against ever silently shipping fabricated LMPE
// compatibility. If the real ETSI TS 103 698 format is confirmed in a
// future task, these assertions must be consciously updated alongside a
// real codec implementation, not accidentally flipped.
class TestLmpeCodecUnconfirmed : public QObject
{
    Q_OBJECT

private slots:
    void reportsFormatNotConfirmed()
    {
        UnconfirmedLmpeCodec codec;
        QCOMPARE(codec.isFormatConfirmed(), false);
        QCOMPARE(codec.formatName(), QStringLiteral("unconfirmed"));
    }

    void encodeAlwaysFails()
    {
        UnconfirmedLmpeCodec codec;
        const auto result = codec.encode(QByteArrayLiteral("payload"));
        QCOMPARE(result.success, false);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.data.isEmpty());
    }

    void decodeAlwaysFails()
    {
        UnconfirmedLmpeCodec codec;
        const auto result = codec.decode(QByteArrayLiteral("wire-bytes"));
        QCOMPARE(result.success, false);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.data.isEmpty());
    }

    void decodeOfEmptyInputAlsoFails()
    {
        UnconfirmedLmpeCodec codec;
        const auto result = codec.decode(QByteArray());
        QCOMPARE(result.success, false);
    }
};

QTEST_APPLESS_MAIN(TestLmpeCodecUnconfirmed)
#include "test_lmpe_codec_unconfirmed.moc"
