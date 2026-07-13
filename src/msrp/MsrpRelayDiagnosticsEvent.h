#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

// One RFC 4976 relay diagnostic event (Task W107) — feeds the relay
// diagnostics UI panel and the msrpRelayEvents JSON export array. Mirrors
// MsrpDiagnosticsEvent's redaction discipline: nonce/response/credential
// values are NEVER stored here in full — see
// docs/msrp-relay-security.md ("what must never be logged").
struct MsrpRelayDiagnosticsEvent
{
    enum class Kind
    {
        Connect,
        ConnectFailed,
        InitialAuthSent,
        ChallengeReceived,
        AuthenticatedAuthSent,
        AllocationSuccess,
        AllocationFailure,
        Refresh,
        RefreshFailure,
        Reconnect,
        Reauthenticate,
        Expired,
        Closed,
        Error
    };

    QDateTime timestamp;
    Kind kind{Kind::Connect};

    QString relayConnectionId;   // internal correlation id only
    QString allocationId;        // internal correlation id only
    QString sipCallIdRedacted;   // truncated/hashed — never the full Call-ID
    int mediaIndex{-1};

    int responseCode{0};
    QString responseComment;
    QString algorithm;           // e.g. "MD5" — never the digest/response itself
    bool qopUsed{false};
    QString allocatedPathRedacted; // host:port only, session-id/token stripped
    QDateTime expiresAt;
    int refreshCount{0};
    int reconnectCount{0};
    QString failureCategory;     // "network" / "auth" / "protocol" / "timeout"
    QString fallbackReason;
    QString error;
};

Q_DECLARE_METATYPE(MsrpRelayDiagnosticsEvent)
