#pragma once
#include <QString>

namespace SipUriNormalizer {

struct Result {
    QString uri;
    bool    isValid{false};
    QString error;
};

// Normalizes a user-entered SIP address into a fully-qualified SIP URI.
//
// Rules (applied in order):
//   • Empty / whitespace-only       → invalid
//   • Contains a space              → invalid (malformed)
//   • Starts with "sip:" or "sips:" → accepted as-is (case-insensitive)
//   • Contains "@"                  → prepend "sip:"
//   • No "@", fallbackDomain given  → "sip:<input>@<fallbackDomain>"
//   • No "@", no fallbackDomain     → invalid
Result normalize(const QString &input, const QString &fallbackDomain = {});

} // namespace SipUriNormalizer
