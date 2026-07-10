#include <QtTest/QtTest>

#include "sip/DeflateDecoder.h"

// Tests for DeflateDecoder — pure Qt, no PJSIP/network needed. Uses Qt's own
// qCompress() to produce valid zlib/deflate fixtures (qCompress output is a
// 4-byte length prefix followed by exactly the zlib stream this decoder
// expects on the wire, so stripping the prefix gives a realistic fixture).

class TestDeflateDecoder : public QObject
{
    Q_OBJECT

private slots:
    void decodesValidZlibStream();
    void emptyInputIsNotOk();
    void garbageInputFails();
    void roundTripsUtf8Text();
};

void TestDeflateDecoder::decodesValidZlibStream()
{
    const QByteArray original = "<imdn><message-id>abc</message-id></imdn>";
    const QByteArray zlibStream = qCompress(original).mid(4); // strip Qt's own length prefix

    const DeflateDecoder::Result result = DeflateDecoder::decode(zlibStream);
    QVERIFY(result.ok);
    QCOMPARE(result.data, original);
}

void TestDeflateDecoder::emptyInputIsNotOk()
{
    const DeflateDecoder::Result result = DeflateDecoder::decode(QByteArray());
    QVERIFY(!result.ok);
    QVERIFY(result.data.isEmpty());
}

void TestDeflateDecoder::garbageInputFails()
{
    const DeflateDecoder::Result result = DeflateDecoder::decode(
        QByteArrayLiteral("this is not a deflate stream at all"));
    QVERIFY(!result.ok);
    QVERIFY(result.data.isEmpty());
}

void TestDeflateDecoder::roundTripsUtf8Text()
{
    const QByteArray original = QStringLiteral("café déjà vu").toUtf8();
    const QByteArray zlibStream = qCompress(original).mid(4);

    const DeflateDecoder::Result result = DeflateDecoder::decode(zlibStream);
    QVERIFY(result.ok);
    QCOMPARE(result.data, original);
}

QTEST_GUILESS_MAIN(TestDeflateDecoder)
#include "test_deflate_decoder.moc"
