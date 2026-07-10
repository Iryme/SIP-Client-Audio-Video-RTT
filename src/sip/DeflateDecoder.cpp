#include "DeflateDecoder.h"

namespace DeflateDecoder {

Result decode(const QByteArray &compressed)
{
    Result result;
    if (compressed.isEmpty())
        return result;

    // qUncompress() expects Qt's own qCompress() framing: a 4-byte
    // big-endian "expected size" hint followed by a zlib stream. The hint
    // is only a starting buffer size — qUncompress() doubles it and retries
    // on Z_BUF_ERROR until it succeeds or zlib reports real corruption
    // (Z_DATA_ERROR) — so any positive value works here; we synthesize the
    // header ourselves since the wire bytes are a bare zlib stream with no
    // such prefix.
    QByteArray framed;
    framed.reserve(compressed.size() + 4);
    const quint32 hint = static_cast<quint32>(compressed.size());
    framed.append(static_cast<char>((hint >> 24) & 0xFF));
    framed.append(static_cast<char>((hint >> 16) & 0xFF));
    framed.append(static_cast<char>((hint >> 8) & 0xFF));
    framed.append(static_cast<char>(hint & 0xFF));
    framed.append(compressed);

    const QByteArray decoded = qUncompress(framed);
    if (decoded.isEmpty())
        return result; // malformed/corrupt stream — qUncompress() failed

    result.ok   = true;
    result.data = decoded;
    return result;
}

} // namespace DeflateDecoder
