#pragma once
#include <QString>

// Classifies the Content-Type of a SIP MESSAGE body (or a CPIM-wrapped inner
// body) for the Messaging Diagnostics feature. Detection is header-string
// based only — it does not validate the body itself.
enum class MessagingContentKind
{
    PlainText,   // text/plain
    Html,        // text/html
    Cpim,        // message/cpim
    Imdn,        // message/imdn+xml
    IsComposing, // application/im-iscomposing+xml
    Sdp,         // application/sdp
    Unknown
};

// Pure Qt/text-based, unit-testable without a live SIP stack.
class MessagingContentKindDetector
{
public:
    static MessagingContentKind detect(const QString &contentType);
    static QString toString(MessagingContentKind kind);
};
