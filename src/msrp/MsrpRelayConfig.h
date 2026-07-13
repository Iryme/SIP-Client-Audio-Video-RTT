#pragma once
#include <QString>

// RFC 4976 MSRP relay configuration (Task W107). Every field is populated
// by the caller (SIP profile / app settings / UI / environment) — nothing
// here is ever hardcoded to a specific provider, host, port, or account.
// Disabled by default (see docs/msrp-relay-security.md: "Experimental").
enum class MsrpRelayMode
{
    Disabled,   // MSRP direct only; relay code path never runs.
    Automatic,  // Try relay if configured; MessagingTransportPolicy decides
                // whether a failed/unavailable allocation may fall back.
    Required    // Never send MSRP without a valid relay allocation.
};

struct MsrpRelayConfig
{
    MsrpRelayMode mode{MsrpRelayMode::Disabled};

    // Relay control-connection target. Left blank unless supplied by the
    // caller (e.g. resolved from a SIP account profile's own relay field,
    // or DNS SRV discovery performed by the caller — this struct never
    // resolves DNS itself).
    QString relayHost;
    int relayPort{0};          // 0 = use MsrpPath default for the chosen scheme
    bool useTls{true};

    // Credentials are never stored here — only a lookup key into
    // CredentialStore, mirroring how SipProfile/XcapServerConfig do it.
    QString credentialProfileId; // CredentialStore profileId, caller-supplied
    QString username;

    bool tlsVerifyPeer{true};
    QString tlsCaCertificatePath;

    int connectTimeoutMs{5000};
    int authTimeoutMs{5000};
    int refreshMarginSeconds{30};   // refresh this many seconds before expiry
    int maxRetries{3};

    bool isUsable() const
    {
        return mode != MsrpRelayMode::Disabled && !relayHost.isEmpty() && !username.isEmpty();
    }
};
