#pragma once
#include <QByteArray>

// Decodes bodies sent with "Content-Encoding: deflate" (RFC 2616 §3.5
// "deflate" coding == a zlib/RFC 1950-wrapped DEFLATE stream — the format
// produced by common SIP UAs, e.g. Linphone, for compressed IMDN/CPIM/
// is-composing bodies). Pure Qt: reuses Qt's own zlib linkage via
// qUncompress() instead of adding a new third-party dependency.
namespace DeflateDecoder {

struct Result
{
    bool       ok{false};
    QByteArray data;
};

// compressed must be the raw wire bytes of the body (no framing). Returns
// ok=false (with empty data) if the bytes are not a valid zlib/deflate
// stream; never throws, never blocks.
Result decode(const QByteArray &compressed);

} // namespace DeflateDecoder
