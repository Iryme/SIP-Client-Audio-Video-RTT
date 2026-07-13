#pragma once
#include <QDateTime>
#include <QList>
#include <QString>

#include "msrp/MsrpPath.h"

// Result of a successful RFC 4976 relay reservation: the Use-Path returned
// by the relay's 200-OK AUTH response, plus bookkeeping needed to refresh
// or invalidate it. Never constructed with an invented/guessed path —
// only MsrpRelayClient::handleAuthResponse() produces one, from real
// relay-response bytes.
struct MsrpRelayAllocation
{
    bool valid{false};

    // Use-Path as returned by the relay: first entry is this client's
    // newly-reserved relay-side URI (what gets offered in SDP a=path);
    // remaining entries (if any) are further relay hops.
    QList<MsrpUri> usePath;

    QDateTime allocatedAt;
    QDateTime expiresAt;

    // Internal correlation identifiers only — never sent on the wire, never
    // shown in full in UI/logs (see MsrpRelayDiagnosticsEvent).
    QString allocationId;
    QString relayConnectionId;

    MsrpUri allocatedUri() const { return usePath.isEmpty() ? MsrpUri() : usePath.first(); }

    bool isExpired(const QDateTime &now) const { return !valid || !expiresAt.isValid() || now >= expiresAt; }

    bool needsRefresh(const QDateTime &now, int marginSeconds) const
    {
        if (!valid || !expiresAt.isValid())
            return false;
        return now.addSecs(marginSeconds) >= expiresAt;
    }
};
