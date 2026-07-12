#include "MessagingTransportPolicy.h"

namespace MessagingTransportPolicy {

Decision decideInitialTransport(MessagingTransportMode mode, bool msrpEstablished, bool allowFallback)
{
    Decision d;

    switch (mode) {
    case MessagingTransportMode::SipMessageOnly:
        d.allowed = true;
        d.transport = MessagingActualTransport::SipMessage;
        return d;

    case MessagingTransportMode::MsrpRequired:
        if (msrpEstablished) {
            d.allowed = true;
            d.transport = MessagingActualTransport::Msrp;
        } else {
            d.allowed = false;
            d.reason = QStringLiteral("MSRP required but no established session is available");
        }
        return d;

    case MessagingTransportMode::MsrpPreferred:
    case MessagingTransportMode::Automatic:
        if (msrpEstablished) {
            d.allowed = true;
            d.transport = MessagingActualTransport::Msrp;
        } else if (allowFallback) {
            d.allowed = true;
            d.transport = MessagingActualTransport::SipMessageFallback;
        } else {
            d.allowed = false;
            d.reason = QStringLiteral("MSRP not established and SIP MESSAGE fallback is disabled");
        }
        return d;
    }

    return d;
}

Decision decideFallbackAfterMsrpFailure(MessagingTransportMode mode, bool allowFallback)
{
    Decision d;

    if (mode == MessagingTransportMode::MsrpRequired) {
        d.allowed = false;
        d.reason = QStringLiteral("MSRP required: no fallback permitted after failure");
        return d;
    }
    if (mode == MessagingTransportMode::SipMessageOnly) {
        // Not a meaningful call (SIP MESSAGE was already the only choice),
        // but answer consistently rather than asserting.
        d.allowed = true;
        d.transport = MessagingActualTransport::SipMessage;
        return d;
    }
    if (!allowFallback) {
        d.allowed = false;
        d.reason = QStringLiteral("SIP MESSAGE fallback is disabled");
        return d;
    }

    d.allowed = true;
    d.transport = MessagingActualTransport::SipMessageFallback;
    return d;
}

} // namespace MessagingTransportPolicy
