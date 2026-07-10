#pragma once
#include <QByteArray>
#include <QString>

// Reusable, dependency-free decoder for HTTP/SIP "Content-Encoding: deflate"
// payloads (and, opportunistically, gzip when the wire bytes carry an
// explicit gzip signature). No third-party zlib is linked: both the
// zlib-wrapped (RFC 1950) and raw (RFC 1951) DEFLATE formats are decoded by
// a small self-contained inflate implementation, which also lets every
// safety limit below be enforced *during* decompression (bounded memory use
// at every step) rather than only after the fact — the standard mitigation
// against a "decompression bomb" (a tiny compressed input that would expand
// to gigabytes).
namespace DeflateDecoder {

enum class Variant { None, Zlib, RawDeflate, Gzip };

enum class ErrorCode {
    None,
    EmptyInput,
    InputTooLarge,
    OutputTooLarge,
    RatioExceeded,
    Corrupt,
    Unsupported,
};

QString variantToString(Variant v);
QString errorCodeToString(ErrorCode e);

struct Limits
{
    qint64 maxCompressedInput{4 * 1024 * 1024};      // 4 MiB
    qint64 maxDecompressedOutput{16 * 1024 * 1024};  // 16 MiB
    qint64 maxExpansionRatio{1024};                  // decoded bytes <= ratio * compressed bytes
};

struct Result
{
    bool       ok{false};
    QByteArray data;
    Variant    variant{Variant::None};
    ErrorCode  errorCode{ErrorCode::None};
    QString    errorMessage; // human-readable; empty when ok
    qint64     inputSize{0};
    qint64     outputSize{0};
};

// compressed: raw wire bytes of the body (binary-safe — must NOT have been
// round-tripped through a text/UTF-8 aware transform first).
Result decode(const QByteArray &compressed, const Limits &limits = Limits());

} // namespace DeflateDecoder
