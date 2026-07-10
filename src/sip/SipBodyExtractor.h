#pragma once
#include <QByteArray>
#include <QString>

// Extracts the exact wire bytes of a SIP message body, independent of any
// text-oriented processing (CRLF normalization, trimming, UTF-8 decoding)
// that would corrupt a binary (e.g. deflate-compressed) body.
//
// SipMessageTrace::rawSip is a QString built via QString::fromLatin1() over
// the original wire bytes (see PjsipTraceModule.cpp) — Latin-1 maps every
// byte 0-255 to one QChar losslessly, so rawSip.toLatin1() recovers the
// exact original bytes. SipBodyExtractor operates on those recovered bytes
// directly: it locates the header/body separator and the Content-Length
// header as byte sequences (never through QString::split/trimmed, which are
// text operations that would corrupt a binary payload or misbehave on an
// embedded NUL byte), and returns exactly the number of body bytes
// Content-Length declares (falling back to "everything after the separator"
// only when Content-Length is absent or unusable).
namespace SipBodyExtractor {

struct Result
{
    bool       headerFound{false}; // a header/body separator was located
    QByteArray headerBlock;        // bytes before the separator (headers only)
    QByteArray rawBodyBytes;       // exact body bytes, Content-Length accurate
    bool       contentLengthUsed{false}; // true if Content-Length bounded the body
};

// rawSip: the full raw SIP text as stored on SipMessageTrace/MessagingTraceEntry
// (Latin-1 round-trip of the original wire bytes).
Result extract(const QString &rawSip);

} // namespace SipBodyExtractor
