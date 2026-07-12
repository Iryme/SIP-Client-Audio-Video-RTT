#pragma once
#include <QString>

// MSRP Foundation (Task W100) — shared enums. Pure Qt/text, no PJSIP types,
// no network access. See docs/msrp-foundation.md for the architecture.

enum class MsrpTransportProtocol { Unknown, Tcp, Tls };

inline QString msrpTransportProtocolToString(MsrpTransportProtocol p)
{
    switch (p) {
    case MsrpTransportProtocol::Tcp: return QStringLiteral("TCP/MSRP");
    case MsrpTransportProtocol::Tls: return QStringLiteral("TCP/TLS/MSRP");
    default: return QStringLiteral("unknown");
    }
}

// RFC 4145 connection-oriented setup role.
enum class MsrpSetup { Unknown, Active, Passive, ActPass, HoldConn };

inline QString msrpSetupToString(MsrpSetup s)
{
    switch (s) {
    case MsrpSetup::Active:   return QStringLiteral("active");
    case MsrpSetup::Passive:  return QStringLiteral("passive");
    case MsrpSetup::ActPass:  return QStringLiteral("actpass");
    case MsrpSetup::HoldConn: return QStringLiteral("holdconn");
    default: return QStringLiteral("unknown");
    }
}

inline MsrpSetup msrpSetupFromString(const QString &raw)
{
    const QString v = raw.trimmed().toLower();
    if (v == QLatin1String("active"))   return MsrpSetup::Active;
    if (v == QLatin1String("passive"))  return MsrpSetup::Passive;
    if (v == QLatin1String("actpass"))  return MsrpSetup::ActPass;
    if (v == QLatin1String("holdconn")) return MsrpSetup::HoldConn;
    return MsrpSetup::Unknown;
}

// a=sendrecv / a=sendonly / a=recvonly / a=inactive
enum class MsrpDirection { Unknown, SendRecv, SendOnly, RecvOnly, Inactive };

inline QString msrpDirectionToString(MsrpDirection d)
{
    switch (d) {
    case MsrpDirection::SendRecv: return QStringLiteral("sendrecv");
    case MsrpDirection::SendOnly: return QStringLiteral("sendonly");
    case MsrpDirection::RecvOnly: return QStringLiteral("recvonly");
    case MsrpDirection::Inactive: return QStringLiteral("inactive");
    default: return QStringLiteral("unknown");
    }
}

// Session lifecycle — deliberately more granular than "negotiated"/
// "connected" so a session is never marked established just because
// m=message appeared in an SDP (task requirement C).
enum class MsrpSessionState {
    Disabled, Detected, Offered, Answered, Negotiated,
    Connecting, Connected, Established,
    Disconnecting, Closed, Failed
};

inline QString msrpSessionStateToString(MsrpSessionState s)
{
    switch (s) {
    case MsrpSessionState::Disabled:      return QStringLiteral("disabled");
    case MsrpSessionState::Detected:      return QStringLiteral("detected");
    case MsrpSessionState::Offered:       return QStringLiteral("offered");
    case MsrpSessionState::Answered:      return QStringLiteral("answered");
    case MsrpSessionState::Negotiated:    return QStringLiteral("negotiated");
    case MsrpSessionState::Connecting:    return QStringLiteral("connecting");
    case MsrpSessionState::Connected:     return QStringLiteral("connected");
    case MsrpSessionState::Established:   return QStringLiteral("established");
    case MsrpSessionState::Disconnecting: return QStringLiteral("disconnecting");
    case MsrpSessionState::Closed:        return QStringLiteral("closed");
    case MsrpSessionState::Failed:        return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

enum class MsrpRole { Unknown, ActiveConnector, PassiveListener, HoldConn };

inline QString msrpRoleToString(MsrpRole r)
{
    switch (r) {
    case MsrpRole::ActiveConnector: return QStringLiteral("active-connector");
    case MsrpRole::PassiveListener: return QStringLiteral("passive-listener");
    case MsrpRole::HoldConn:        return QStringLiteral("holdconn");
    default: return QStringLiteral("unknown");
    }
}

enum class MsrpParseStatus { Ok, Partial, Error };

inline QString msrpParseStatusToString(MsrpParseStatus s)
{
    switch (s) {
    case MsrpParseStatus::Ok:      return QStringLiteral("ok");
    case MsrpParseStatus::Partial: return QStringLiteral("partial");
    case MsrpParseStatus::Error:   return QStringLiteral("error");
    }
    return QStringLiteral("error");
}

// RFC 4975 continuation flag, the byte immediately following the final
// CRLF of the end-line delimiter.
enum class MsrpContinuation { Unknown, More /* '+' */, Complete /* '$' */, Abort /* '#' */ };

inline QChar msrpContinuationToChar(MsrpContinuation c)
{
    switch (c) {
    case MsrpContinuation::More:     return QLatin1Char('+');
    case MsrpContinuation::Complete: return QLatin1Char('$');
    case MsrpContinuation::Abort:    return QLatin1Char('#');
    default: return QLatin1Char('$');
    }
}

inline MsrpContinuation msrpContinuationFromChar(QChar c)
{
    if (c == QLatin1Char('+')) return MsrpContinuation::More;
    if (c == QLatin1Char('$')) return MsrpContinuation::Complete;
    if (c == QLatin1Char('#')) return MsrpContinuation::Abort;
    return MsrpContinuation::Unknown;
}

enum class MsrpMethod { Unknown, Send, Report, Response };

inline QString msrpMethodToString(MsrpMethod m)
{
    switch (m) {
    case MsrpMethod::Send:     return QStringLiteral("SEND");
    case MsrpMethod::Report:   return QStringLiteral("REPORT");
    case MsrpMethod::Response: return QStringLiteral("response");
    default: return QStringLiteral("unknown");
    }
}

// Transaction lifecycle (task section J).
enum class MsrpTransactionStatus {
    Queued, Sending, AwaitingResponse, Accepted,
    ReportedSuccess, ReportedFailure, TimedOut, Aborted, Failed
};

inline QString msrpTransactionStatusToString(MsrpTransactionStatus s)
{
    switch (s) {
    case MsrpTransactionStatus::Queued:           return QStringLiteral("queued");
    case MsrpTransactionStatus::Sending:          return QStringLiteral("sending");
    case MsrpTransactionStatus::AwaitingResponse: return QStringLiteral("awaiting-response");
    case MsrpTransactionStatus::Accepted:         return QStringLiteral("accepted");
    case MsrpTransactionStatus::ReportedSuccess:  return QStringLiteral("reported-success");
    case MsrpTransactionStatus::ReportedFailure:  return QStringLiteral("reported-failure");
    case MsrpTransactionStatus::TimedOut:         return QStringLiteral("timed-out");
    case MsrpTransactionStatus::Aborted:          return QStringLiteral("aborted");
    case MsrpTransactionStatus::Failed:           return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

// Task M — explicit transport selection policy.
enum class MessagingTransportMode { SipMessageOnly, MsrpPreferred, MsrpRequired, Automatic };

inline QString messagingTransportModeToString(MessagingTransportMode m)
{
    switch (m) {
    case MessagingTransportMode::SipMessageOnly: return QStringLiteral("sip-message-only");
    case MessagingTransportMode::MsrpPreferred:  return QStringLiteral("msrp-preferred");
    case MessagingTransportMode::MsrpRequired:   return QStringLiteral("msrp-required");
    case MessagingTransportMode::Automatic:      return QStringLiteral("automatic");
    }
    return QStringLiteral("automatic");
}

inline MessagingTransportMode messagingTransportModeFromString(const QString &raw)
{
    const QString v = raw.trimmed().toLower();
    if (v == QLatin1String("sip-message-only")) return MessagingTransportMode::SipMessageOnly;
    if (v == QLatin1String("msrp-preferred"))   return MessagingTransportMode::MsrpPreferred;
    if (v == QLatin1String("msrp-required"))    return MessagingTransportMode::MsrpRequired;
    return MessagingTransportMode::Automatic;
}

enum class MessagingActualTransport { SipMessage, Msrp, SipMessageFallback };

inline QString messagingActualTransportToString(MessagingActualTransport t)
{
    switch (t) {
    case MessagingActualTransport::Msrp:                return QStringLiteral("msrp");
    case MessagingActualTransport::SipMessageFallback:  return QStringLiteral("sip-message-fallback");
    default:                                            return QStringLiteral("sip-message");
    }
}
