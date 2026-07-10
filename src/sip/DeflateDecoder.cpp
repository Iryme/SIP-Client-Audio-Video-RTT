#include "DeflateDecoder.h"

#include <QVector>
#include <algorithm>

namespace {

// ---------------------------------------------------------------------
// Minimal RFC 1951 (raw DEFLATE) bit-oriented reader + canonical Huffman
// decoder, structured after the well-known public-domain "puff.c"
// reference algorithm (Mark Adler / zlib project), rewritten in C++ against
// QByteArray. This is the single decompression primitive both the
// zlib-wrapped (RFC 1950 = 2-byte header + raw DEFLATE + 4-byte Adler32,
// trailer not verified) and the raw-DEFLATE / gzip-payload paths funnel
// through, so every output byte written is checked against the caller's
// size limit as it is produced.
// ---------------------------------------------------------------------

struct BitReader
{
    const unsigned char *in;
    qint64 inLen;
    qint64 pos{0};
    quint32 buf{0};
    int     cnt{0};
    bool    overrun{false};

    int bit()
    {
        if (cnt == 0) {
            if (pos >= inLen) {
                overrun = true;
                return 0;
            }
            buf = in[pos++];
            cnt = 8;
        }
        const int b = buf & 1;
        buf >>= 1;
        --cnt;
        return b;
    }

    // Reads n bits, LSB first. Returns -1 on overrun.
    long bits(int n)
    {
        long value = 0;
        for (int i = 0; i < n; ++i)
            value |= static_cast<long>(bit()) << i;
        return overrun ? -1 : value;
    }

    void alignToByte()
    {
        buf = 0;
        cnt = 0;
    }
};

struct Huffman
{
    QVector<short> count;  // count[len] = number of codes of that bit length
    QVector<short> symbol; // symbols, ordered by code within each length
};

// Builds a canonical Huffman decode table from per-symbol code lengths.
// Returns false if the length set is over-subscribed (invalid stream).
bool buildHuffman(Huffman &h, const QVector<short> &lengths, int numSymbols)
{
    h.count.assign(16, 0);
    for (int sym = 0; sym < numSymbols; ++sym)
        h.count[lengths[sym]]++;

    int left = 1;
    for (int len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0)
            return false; // over-subscribed
    }

    QVector<short> offs(16, 0);
    for (int len = 1; len < 15; ++len)
        offs[len + 1] = offs[len] + h.count[len];

    h.symbol.assign(numSymbols, 0);
    for (int sym = 0; sym < numSymbols; ++sym) {
        if (lengths[sym] != 0)
            h.symbol[offs[lengths[sym]]++] = static_cast<short>(sym);
    }
    return true;
}

int decodeSymbol(BitReader &br, const Huffman &h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; ++len) {
        code |= br.bit();
        if (br.overrun)
            return -1;
        const int count = h.count[len];
        if (code - first < count)
            return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

const short kLengthBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
const short kLengthExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
const short kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
const short kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
const int kCodeLengthOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// Inflates a raw (headerless) RFC 1951 DEFLATE stream. `maxOut` bounds the
// output buffer growth — exceeding it aborts decoding immediately and sets
// limitExceeded, before any further (potentially unbounded) allocation.
bool inflateRawDeflate(const unsigned char *data, qint64 len, QByteArray &out,
                        qint64 maxOut, bool &limitExceeded)
{
    BitReader br{data, len};
    limitExceeded = false;

    bool final = false;
    do {
        final = br.bit() != 0;
        if (br.overrun)
            return false;
        const long type = br.bits(2);
        if (type < 0)
            return false;

        if (type == 0) {
            br.alignToByte();
            if (br.pos + 4 > br.inLen)
                return false;
            const unsigned len16  = static_cast<unsigned char>(data[br.pos])
                                   | (static_cast<unsigned char>(data[br.pos + 1]) << 8);
            const unsigned nlen16 = static_cast<unsigned char>(data[br.pos + 2])
                                   | (static_cast<unsigned char>(data[br.pos + 3]) << 8);
            br.pos += 4;
            if ((len16 ^ 0xFFFFu) != nlen16)
                return false;
            if (br.pos + static_cast<qint64>(len16) > br.inLen)
                return false;
            if (static_cast<qint64>(out.size()) + len16 > maxOut) {
                limitExceeded = true;
                return false;
            }
            out.append(reinterpret_cast<const char *>(data + br.pos), static_cast<int>(len16));
            br.pos += len16;
        } else if (type == 1 || type == 2) {
            Huffman litHuff, distHuff;

            if (type == 1) {
                QVector<short> litLens(288);
                int sym = 0;
                for (; sym < 144; ++sym) litLens[sym] = 8;
                for (; sym < 256; ++sym) litLens[sym] = 9;
                for (; sym < 280; ++sym) litLens[sym] = 7;
                for (; sym < 288; ++sym) litLens[sym] = 8;
                QVector<short> distLens(30, 5);
                if (!buildHuffman(litHuff, litLens, 288) || !buildHuffman(distHuff, distLens, 30))
                    return false;
            } else {
                const long hlitRaw = br.bits(5);
                const long hdistRaw = br.bits(5);
                const long hclenRaw = br.bits(4);
                if (hlitRaw < 0 || hdistRaw < 0 || hclenRaw < 0)
                    return false;
                const int hlit = static_cast<int>(hlitRaw) + 257;
                const int hdist = static_cast<int>(hdistRaw) + 1;
                const int hclen = static_cast<int>(hclenRaw) + 4;

                QVector<short> clLens(19, 0);
                for (int i = 0; i < hclen; ++i) {
                    const long v = br.bits(3);
                    if (v < 0)
                        return false;
                    clLens[kCodeLengthOrder[i]] = static_cast<short>(v);
                }
                Huffman clHuff;
                if (!buildHuffman(clHuff, clLens, 19))
                    return false;

                const int totalLens = hlit + hdist;
                QVector<short> lens(totalLens, 0);
                int i = 0;
                while (i < totalLens) {
                    const int sym = decodeSymbol(br, clHuff);
                    if (sym < 0)
                        return false;
                    if (sym < 16) {
                        lens[i++] = static_cast<short>(sym);
                    } else if (sym == 16) {
                        if (i == 0)
                            return false;
                        long rep = br.bits(2);
                        if (rep < 0)
                            return false;
                        rep += 3;
                        const short prev = lens[i - 1];
                        while (rep-- > 0 && i < totalLens)
                            lens[i++] = prev;
                    } else if (sym == 17) {
                        long rep = br.bits(3);
                        if (rep < 0)
                            return false;
                        rep += 3;
                        while (rep-- > 0 && i < totalLens)
                            lens[i++] = 0;
                    } else { // sym == 18
                        long rep = br.bits(7);
                        if (rep < 0)
                            return false;
                        rep += 11;
                        while (rep-- > 0 && i < totalLens)
                            lens[i++] = 0;
                    }
                }

                QVector<short> litLens = lens.mid(0, hlit);
                QVector<short> distLens = lens.mid(hlit, hdist);
                if (!buildHuffman(litHuff, litLens, hlit) || !buildHuffman(distHuff, distLens, hdist))
                    return false;
            }

            while (true) {
                const int sym = decodeSymbol(br, litHuff);
                if (sym < 0)
                    return false;
                if (sym < 256) {
                    if (static_cast<qint64>(out.size()) + 1 > maxOut) {
                        limitExceeded = true;
                        return false;
                    }
                    out.append(static_cast<char>(sym));
                } else if (sym == 256) {
                    break;
                } else {
                    const int lenSym = sym - 257;
                    if (lenSym < 0 || lenSym >= 29)
                        return false;
                    long extra = kLengthExtra[lenSym] ? br.bits(kLengthExtra[lenSym]) : 0;
                    if (extra < 0)
                        return false;
                    const int length = kLengthBase[lenSym] + static_cast<int>(extra);

                    const int distSym = decodeSymbol(br, distHuff);
                    if (distSym < 0 || distSym >= 30)
                        return false;
                    long dextra = kDistExtra[distSym] ? br.bits(kDistExtra[distSym]) : 0;
                    if (dextra < 0)
                        return false;
                    const int distance = kDistBase[distSym] + static_cast<int>(dextra);

                    if (distance > out.size())
                        return false; // invalid back-reference

                    if (static_cast<qint64>(out.size()) + length > maxOut) {
                        limitExceeded = true;
                        return false;
                    }
                    const int startPos = out.size() - distance;
                    for (int k = 0; k < length; ++k)
                        out.append(out.at(startPos + k));
                }
            }
        } else {
            return false; // reserved block type (11)
        }
    } while (!final);

    return true;
}

bool looksLikeZlibHeader(const QByteArray &data)
{
    if (data.size() < 2)
        return false;
    const unsigned cmf = static_cast<unsigned char>(data[0]);
    const unsigned flg = static_cast<unsigned char>(data[1]);
    if ((cmf & 0x0F) != 8) // compression method must be DEFLATE
        return false;
    return ((cmf << 8) | flg) % 31 == 0; // RFC 1950 header check bits
}

// RFC 1952 gzip: fixed 10-byte header, optional FEXTRA/FNAME/FCOMMENT/FHCRC
// fields, then a raw DEFLATE stream. Returns the byte offset where the
// DEFLATE data begins, or -1 if the header is malformed/unsupported.
int gzipDeflateStart(const QByteArray &data)
{
    if (data.size() < 10)
        return -1;
    if (static_cast<unsigned char>(data[0]) != 0x1F || static_cast<unsigned char>(data[1]) != 0x8B)
        return -1;
    if (static_cast<unsigned char>(data[2]) != 8) // CM must be DEFLATE
        return -1;

    const unsigned char flg = static_cast<unsigned char>(data[3]);
    int pos = 10;

    if (flg & 0x04) { // FEXTRA
        if (pos + 2 > data.size())
            return -1;
        const unsigned xlen = static_cast<unsigned char>(data[pos]) | (static_cast<unsigned char>(data[pos + 1]) << 8);
        pos += 2 + static_cast<int>(xlen);
        if (pos > data.size())
            return -1;
    }
    if (flg & 0x08) { // FNAME (NUL-terminated)
        const int nul = data.indexOf('\0', pos);
        if (nul < 0)
            return -1;
        pos = nul + 1;
    }
    if (flg & 0x10) { // FCOMMENT (NUL-terminated)
        const int nul = data.indexOf('\0', pos);
        if (nul < 0)
            return -1;
        pos = nul + 1;
    }
    if (flg & 0x02) { // FHCRC
        pos += 2;
    }
    return pos <= data.size() ? pos : -1;
}

} // namespace

namespace DeflateDecoder {

QString variantToString(Variant v)
{
    switch (v) {
    case Variant::Zlib:       return QStringLiteral("zlib");
    case Variant::RawDeflate: return QStringLiteral("raw-deflate");
    case Variant::Gzip:       return QStringLiteral("gzip");
    case Variant::None:       break;
    }
    return QStringLiteral("none");
}

QString errorCodeToString(ErrorCode e)
{
    switch (e) {
    case ErrorCode::EmptyInput:     return QStringLiteral("empty-input");
    case ErrorCode::InputTooLarge:  return QStringLiteral("input-too-large");
    case ErrorCode::OutputTooLarge: return QStringLiteral("output-too-large");
    case ErrorCode::RatioExceeded:  return QStringLiteral("expansion-ratio-exceeded");
    case ErrorCode::Corrupt:        return QStringLiteral("corrupt");
    case ErrorCode::Unsupported:    return QStringLiteral("unsupported");
    case ErrorCode::None:           break;
    }
    return QStringLiteral("none");
}

Result decode(const QByteArray &compressed, const Limits &limits)
{
    Result r;
    r.inputSize = compressed.size();

    if (compressed.isEmpty()) {
        r.errorCode = ErrorCode::EmptyInput;
        r.errorMessage = QStringLiteral("empty input");
        return r;
    }

    if (compressed.size() > limits.maxCompressedInput) {
        r.errorCode = ErrorCode::InputTooLarge;
        r.errorMessage = QStringLiteral("compressed input (%1 bytes) exceeds configured maximum (%2 bytes)")
                              .arg(compressed.size())
                              .arg(limits.maxCompressedInput);
        return r;
    }

    const qint64 ratioCap = static_cast<qint64>(compressed.size()) * limits.maxExpansionRatio;
    const qint64 maxOut = std::min<qint64>(limits.maxDecompressedOutput, ratioCap);
    const bool ratioIsBinding = ratioCap < limits.maxDecompressedOutput;

    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(compressed.constData());

    // gzip is only attempted when the wire bytes carry the explicit 1F 8B
    // signature — never guessed otherwise.
    const int gzipStart = gzipDeflateStart(compressed);
    if (gzipStart >= 0) {
        QByteArray out;
        bool limitExceeded = false;
        const bool success = inflateRawDeflate(bytes + gzipStart, compressed.size() - gzipStart,
                                                out, maxOut, limitExceeded);
        if (success) {
            r.ok = true;
            r.data = out;
            r.variant = Variant::Gzip;
            r.outputSize = out.size();
            return r;
        }
        if (limitExceeded) {
            r.errorCode = ratioIsBinding ? ErrorCode::RatioExceeded : ErrorCode::OutputTooLarge;
            r.errorMessage = QStringLiteral("decompressed gzip output exceeded configured safety limit");
            return r;
        }
        r.errorCode = ErrorCode::Corrupt;
        r.errorMessage = QStringLiteral("deflate decode failed: gzip signature present but the "
                                         "DEFLATE stream inside it is corrupt or truncated");
        return r;
    }

    // Try the zlib-wrapped (RFC 1950) form: 2-byte header, then raw DEFLATE.
    // The 4-byte Adler-32 trailer is not verified — this is a diagnostic
    // decoder, not a correctness-critical transport, and skipping the
    // checksum keeps the implementation self-contained (see class docs).
    if (looksLikeZlibHeader(compressed)) {
        QByteArray out;
        bool limitExceeded = false;
        const bool success = inflateRawDeflate(bytes + 2, compressed.size() - 2, out, maxOut, limitExceeded);
        if (success) {
            r.ok = true;
            r.data = out;
            r.variant = Variant::Zlib;
            r.outputSize = out.size();
            return r;
        }
        if (limitExceeded) {
            r.errorCode = ratioIsBinding ? ErrorCode::RatioExceeded : ErrorCode::OutputTooLarge;
            r.errorMessage = QStringLiteral("decompressed output exceeded configured safety limit");
            return r;
        }
        // Header looked like zlib but the stream itself didn't decode —
        // fall through and try it as raw (headerless) DEFLATE below, since
        // a coincidental 2-byte header match on raw deflate data is
        // possible (RFC 1950's check is only 5 bits wide).
    }

    // Raw (headerless) RFC 1951 DEFLATE fallback.
    {
        QByteArray out;
        bool limitExceeded = false;
        const bool success = inflateRawDeflate(bytes, compressed.size(), out, maxOut, limitExceeded);
        if (success) {
            r.ok = true;
            r.data = out;
            r.variant = Variant::RawDeflate;
            r.outputSize = out.size();
            return r;
        }
        if (limitExceeded) {
            r.errorCode = ratioIsBinding ? ErrorCode::RatioExceeded : ErrorCode::OutputTooLarge;
            r.errorMessage = QStringLiteral("decompressed output exceeded configured safety limit");
            return r;
        }
    }

    r.errorCode = ErrorCode::Corrupt;
    r.errorMessage = QStringLiteral("deflate decode failed: data is not a valid zlib-wrapped or "
                                     "raw DEFLATE stream");
    return r;
}

} // namespace DeflateDecoder
