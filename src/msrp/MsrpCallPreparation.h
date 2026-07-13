#pragma once
#include <QMetaType>
#include <QString>
#include <memory>

#include "msrp/MsrpPath.h"
#include "msrp/MsrpRelayAllocation.h"

class MsrpRelayClient;

// Task W108: which transport this call's MSRP media section will use, once
// call preparation (see MsrpCallPreparationController) has finished.
enum class MsrpCallTransportMode
{
    None,   // MSRP not requested/enabled for this call at all.
    Direct, // Unchanged W100-era direct MSRP: SipCall lazily starts its own
            // passive listener / active-connect from onCallSdpCreated,
            // exactly as before W108. advertisedUri below is unused.
    Relay   // RFC 4976 relay-assisted: advertisedUri/relayAllocation were
            // obtained from a real relay allocation *before* the SDP that
            // offers them was built — never a guessed/invented path.
};

// Immutable-by-convention snapshot handed from MsrpCallPreparationController
// to SipCall right before pjsip's INVITE (or answer) construction begins.
// Every field is already-resolved data — onCallSdpCreated only ever reads
// this struct; it never performs allocation, DNS, or any I/O itself.
struct PreparedMsrpOffer
{
    bool ready{false};
    MsrpCallTransportMode mode{MsrpCallTransportMode::None};

    // Relay mode only: the allocated Use-Path entry to advertise as a=path,
    // and the full allocation record (expiry etc.) for lifecycle tracking.
    MsrpUri advertisedUri;
    MsrpRelayAllocation relayAllocation;

    // Relay mode only: kept alive for the lifetime of the call so that (a)
    // its already-connected/authenticated transport can be adopted into the
    // call's MsrpSession (see MsrpSession::adoptExternalTransport), and (b)
    // its refresh cycle (allocationRefreshed/allocationLost) can still be
    // observed after the call is established. Uses a deleteLater() deleter
    // so dropping the last reference is always safe even from inside one of
    // the client's own signal handlers.
    std::shared_ptr<MsrpRelayClient> relayClient;

    // Preparation correlation id (Task W108 diagnostics) — not a SIP or
    // MSRP protocol identifier, never sent on the wire.
    QString preparationId;
};

// Wraps a QObject* whose lifetime must be ended via deleteLater() (never a
// synchronous delete) in a std::shared_ptr, so it can be safely dropped from
// inside one of the object's own signal handlers without reentrant
// destruction. See docs/msrp-relay-call-preparation.md, "Ownership".
template <typename T>
std::shared_ptr<T> makeDeleteLaterSharedPtr(T *obj)
{
    return std::shared_ptr<T>(obj, [](T *p) {
        if (p)
            p->deleteLater();
    });
}

Q_DECLARE_METATYPE(PreparedMsrpOffer)
