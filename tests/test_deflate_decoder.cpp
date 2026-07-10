#include <QtTest/QtTest>

#include "sip/DeflateDecoder.h"

// Fixtures are built by hand using RFC 1951's "stored" (uncompressed) block
// type — a single byte header (BFINAL=1, BTYPE=00), byte-aligned, then a
// 2-byte LEN + 2-byte one's-complement NLEN, then LEN literal bytes. This
// keeps every fixture deterministic and independent of any compression
// library (no zlib is linked into this project — see DeflateDecoder.h),
// while still exercising the real decode path bit-for-bit.

namespace {

QByteArray storedDeflateBlock(const QByteArray &literal)
{
    QByteArray out;
    out.append(char(0x01)); // BFINAL=1, BTYPE=00 (stored), rest padding
    const quint16 len = static_cast<quint16>(literal.size());
    const quint16 nlen = static_cast<quint16>(~len);
    out.append(char(len & 0xFF));
    out.append(char((len >> 8) & 0xFF));
    out.append(char(nlen & 0xFF));
    out.append(char((nlen >> 8) & 0xFF));
    out.append(literal);
    return out;
}

QByteArray zlibWrap(const QByteArray &rawDeflate)
{
    QByteArray out;
    out.append(char(0x78));
    out.append(char(0x9C)); // valid RFC 1950 header (CMF=0x78, FLG=0x9C, checks out mod 31)
    out.append(rawDeflate);
    out.append(QByteArray(4, '\0')); // Adler-32 trailer — not verified by this decoder
    return out;
}

} // namespace

class TestDeflateDecoder : public QObject
{
    Q_OBJECT

private slots:
    void zlibWrappedDecodes();
    void rawDeflateDecodes();
    void invalidDataFails();
    void truncatedDataFails();
    void emptyInputFails();
    void outputLimitExceeded();
    void expansionRatioLimitExceeded();
    void inputSizeLimitExceeded();
    void gzipSignatureDecodes();
    void bodyWithNullBytesRoundTrips();
};

void TestDeflateDecoder::zlibWrappedDecodes()
{
    const QByteArray original = "<imdn><message-id>abc-123</message-id></imdn>";
    const QByteArray wire = zlibWrap(storedDeflateBlock(original));

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire);
    QVERIFY(r.ok);
    QCOMPARE(r.variant, DeflateDecoder::Variant::Zlib);
    QCOMPARE(r.data, original);
    QCOMPARE(r.outputSize, static_cast<qint64>(original.size()));
    QCOMPARE(r.inputSize, static_cast<qint64>(wire.size()));
}

void TestDeflateDecoder::rawDeflateDecodes()
{
    const QByteArray original = "<isComposing><state>active</state></isComposing>";
    const QByteArray wire = storedDeflateBlock(original);

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire);
    QVERIFY(r.ok);
    QCOMPARE(r.variant, DeflateDecoder::Variant::RawDeflate);
    QCOMPARE(r.data, original);
}

void TestDeflateDecoder::invalidDataFails()
{
    const QByteArray garbage = QByteArrayLiteral("this is not deflate data at all, just plain text");
    const DeflateDecoder::Result r = DeflateDecoder::decode(garbage);
    QVERIFY(!r.ok);
    QCOMPARE(r.errorCode, DeflateDecoder::ErrorCode::Corrupt);
    QVERIFY(!r.errorMessage.isEmpty());
}

void TestDeflateDecoder::truncatedDataFails()
{
    QByteArray wire = zlibWrap(storedDeflateBlock("hello world, this is a longer literal payload"));
    wire.chop(10); // cut off part of the literal data — LEN no longer matches available bytes

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire);
    QVERIFY(!r.ok);
    QCOMPARE(r.errorCode, DeflateDecoder::ErrorCode::Corrupt);
}

void TestDeflateDecoder::emptyInputFails()
{
    const DeflateDecoder::Result r = DeflateDecoder::decode(QByteArray());
    QVERIFY(!r.ok);
    QCOMPARE(r.errorCode, DeflateDecoder::ErrorCode::EmptyInput);
}

void TestDeflateDecoder::outputLimitExceeded()
{
    const QByteArray original(500, 'x');
    const QByteArray wire = storedDeflateBlock(original);

    DeflateDecoder::Limits limits;
    limits.maxDecompressedOutput = 100;
    limits.maxExpansionRatio = 1000; // ratio cap not binding — output cap is

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire, limits);
    QVERIFY(!r.ok);
    QCOMPARE(r.errorCode, DeflateDecoder::ErrorCode::OutputTooLarge);
}

void TestDeflateDecoder::expansionRatioLimitExceeded()
{
    // Stored (uncompressed) DEFLATE blocks can never actually expand
    // (output size < wire size, header overhead included), so a realistic
    // ratio can't be exceeded with this fixture format. maxExpansionRatio=0
    // instead forces the *effective* output cap to 0 regardless of the
    // (much larger) maxDecompressedOutput — proving the ratio cap, not the
    // absolute cap, is what triggers the rejection (RatioExceeded, not
    // OutputTooLarge) below.
    const QByteArray original(50, 'y');
    const QByteArray wire = storedDeflateBlock(original);

    DeflateDecoder::Limits limits;
    limits.maxDecompressedOutput = 1024 * 1024; // output cap not binding
    limits.maxExpansionRatio = 0;               // ratio cap is binding: 0 * wire.size() = 0

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire, limits);
    QVERIFY(!r.ok);
    QCOMPARE(r.errorCode, DeflateDecoder::ErrorCode::RatioExceeded);
}

void TestDeflateDecoder::inputSizeLimitExceeded()
{
    const QByteArray wire = storedDeflateBlock(QByteArray(50, 'z'));

    DeflateDecoder::Limits limits;
    limits.maxCompressedInput = 10; // smaller than wire.size()

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire, limits);
    QVERIFY(!r.ok);
    QCOMPARE(r.errorCode, DeflateDecoder::ErrorCode::InputTooLarge);
}

void TestDeflateDecoder::gzipSignatureDecodes()
{
    const QByteArray original = "gzip-wrapped diagnostic payload";
    QByteArray wire;
    wire.append(char(0x1F));
    wire.append(char(0x8B));
    wire.append(char(0x08)); // CM = deflate
    wire.append(char(0x00)); // FLG = none set
    wire.append(QByteArray(4, '\0')); // MTIME
    wire.append(char(0x00)); // XFL
    wire.append(char(0xFF)); // OS = unknown
    wire.append(storedDeflateBlock(original));
    wire.append(QByteArray(8, '\0')); // CRC32 + ISIZE trailer, not verified

    const DeflateDecoder::Result r = DeflateDecoder::decode(wire);
    QVERIFY(r.ok);
    QCOMPARE(r.variant, DeflateDecoder::Variant::Gzip);
    QCOMPARE(r.data, original);
}

void TestDeflateDecoder::bodyWithNullBytesRoundTrips()
{
    QByteArray original;
    original.append("before");
    original.append('\0');
    original.append("after");
    QCOMPARE(original.size(), 12); // "before" + NUL + "after"

    const QByteArray wire = storedDeflateBlock(original);
    const DeflateDecoder::Result r = DeflateDecoder::decode(wire);
    QVERIFY(r.ok);
    QCOMPARE(r.data, original);
}

QTEST_GUILESS_MAIN(TestDeflateDecoder)
#include "test_deflate_decoder.moc"
