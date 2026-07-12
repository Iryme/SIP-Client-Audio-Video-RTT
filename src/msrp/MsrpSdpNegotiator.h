#pragma once
#include <QList>
#include <QString>
#include <QStringList>

#include "msrp/MsrpPath.h"
#include "msrp/MsrpTypes.h"

// MSRP SDP negotiation (Task W100, section D) — pure text-based parsing and
// construction of "m=message" media sections. No network access, no PJSIP
// types. Operates on already-extracted SDP text (e.g. from
// SipTraceLogger/PjsipTraceModule captures, or a manually-composed MSRP
// dialog body — see docs/msrp-foundation.md for why this is not wired into
// pjsua2's live INVITE SDP generation in this task).
namespace MsrpSdpNegotiator {

struct MediaBlock
{
    bool rejected{false};        // port == 0
    int port{0};
    MsrpTransportProtocol transport{MsrpTransportProtocol::Unknown};
    QList<MsrpUri> path;
    QStringList acceptTypes;
    QStringList acceptWrappedTypes;
    MsrpSetup setup{MsrpSetup::Unknown};
    QString connection;          // "new" / "existing", verbatim
    MsrpDirection direction{MsrpDirection::Unknown};
    QString fileSelector;
    QString fileDisposition;
    QString fileTransferId;
    MsrpParseStatus parseStatus{MsrpParseStatus::Ok};
    QStringList warnings;
};

// Finds every "m=message" section in sdpText (order-independent attribute
// parsing within each section) — supports multiple m=message sections per
// SDP, per requirement D.
QList<MediaBlock> parseMessageBlocks(const QString &sdpText);

struct NegotiationResult
{
    bool valid{false};
    MsrpRole role{MsrpRole::Unknown};
    QString errorMessage;
};

// Decides the connection role from two already-chosen (post offer/answer)
// setup values. Rejects active/active and passive/passive combinations
// with a diagnostic rather than silently picking one; an unresolved
// "actpass" on either side means negotiation is not yet complete.
NegotiationResult negotiateRole(MsrpSetup localSetup, MsrpSetup remoteSetup);

// Builds a single "m=message ..." section (CRLF-terminated lines) suitable
// for embedding into an SDP body. host/port/path/session-id are always
// caller-supplied (runtime/config), never hardcoded here.
QString buildOfferBlock(const MsrpUri &localUri, MsrpSetup setup,
                        const QStringList &acceptTypes,
                        const QStringList &acceptWrappedTypes);

} // namespace MsrpSdpNegotiator
