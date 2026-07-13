#pragma once
#include <QString>

// RFC 4976 §5 MSRP relay AUTH digest (HTTP Digest, RFC 2617, reused with
// method="AUTH" and digest-uri = the relay's msrp(s):// URI being
// authenticated to — analogous to SIP digest but not SIP, so pjsip's
// internal digest state machine cannot be reused; see MsrpRelayClient).
// Pure functions only: no I/O, no logging. Callers (MsrpRelayClient) are
// responsible for never logging nonce/response/password values produced or
// consumed here — see docs/msrp-relay-security.md.
namespace MsrpDigestAuth {

struct Challenge
{
    bool ok{false};
    QString realm;
    QString nonce;
    QString algorithm{QStringLiteral("MD5")};
    QString qop;      // "auth" or empty (server may offer none)
    QString opaque;
    bool stale{false};
};

// Parses a "WWW-Authenticate: Digest realm=\"...\", nonce=\"...\", ..." header
// value (the "Digest " scheme prefix is optional on input).
Challenge parseChallenge(const QString &headerValue);

QString computeHA1(const QString &username, const QString &realm, const QString &password);
QString computeHA2(const QString &method, const QString &digestUri);
QString computeResponse(const QString &ha1, const QString &nonce, const QString &nc,
                        const QString &cnonce, const QString &qop, const QString &ha2);

// Cryptographically-random client nonce, hex-encoded.
QString generateCnonce();

struct AuthorizationParams
{
    QString username;
    QString realm;
    QString nonce;
    QString uri;       // digest-uri (relay URI being authenticated to)
    QString response;
    QString algorithm;
    QString cnonce;    // empty when qop is empty
    QString nc;         // 8-hex-digit nonce-count, empty when qop is empty
    QString qop;        // empty when the challenge offered none
    QString opaque;     // empty when the challenge did not supply one
};

// Builds the "Authorization" header value for an authenticated AUTH retry.
QString buildAuthorizationHeader(const AuthorizationParams &params);

// Convenience: computes response + builds the full header value in one call.
// nc must be a caller-tracked, monotonically-increasing counter (as 8 hex
// digits) for reuse of the same nonce across multiple AUTH transactions.
QString buildAuthorizationHeader(const Challenge &challenge, const QString &username,
                                 const QString &password, const QString &digestUri,
                                 const QString &cnonce, const QString &nc);

} // namespace MsrpDigestAuth
