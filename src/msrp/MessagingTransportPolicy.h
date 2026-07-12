#pragma once
#include <QString>

#include "msrp/MsrpTypes.h"

// Explicit messaging transport selection policy (Task W100, section M) —
// pure decision logic, no I/O, fully unit-testable. The caller (composer/
// SipManager integration) is responsible for the actual invariants this
// implies: never sending the same message on both transports at once, and
// never falling back after an MSRP success (this module only ever decides
// what to do *before* a send attempt, or whether a *failed* attempt may
// retry on the other transport — it never re-authorizes a transport that
// already succeeded).
namespace MessagingTransportPolicy {

struct Decision
{
    bool allowed{false};
    MessagingActualTransport transport{MessagingActualTransport::SipMessage};
    QString reason; // explanation, especially for the not-allowed case
};

// msrpEstablished: is there a currently-established MSRP session available
// for this conversation right now. allowFallback: AppSettings::
// allowSipMessageFallback(). Never returns Msrp when mode is
// SipMessageOnly, and never silently substitutes SIP MESSAGE for
// MsrpRequired.
Decision decideInitialTransport(MessagingTransportMode mode, bool msrpEstablished, bool allowFallback);

// Called only after an MSRP send attempt has already failed for this
// specific message. Returns whether a SIP MESSAGE retry is permitted, and
// never returns true for MsrpRequired (an explicit error, not a silent
// fallback) or when allowFallback is false.
Decision decideFallbackAfterMsrpFailure(MessagingTransportMode mode, bool allowFallback);

} // namespace MessagingTransportPolicy
