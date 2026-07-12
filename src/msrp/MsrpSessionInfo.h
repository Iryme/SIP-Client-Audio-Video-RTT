#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include "msrp/MsrpTypes.h"

// MSRP session model (Task W100, section C) — the only MSRP type the UI
// ever binds to directly. Never holds a QTcpSocket/QSslSocket pointer.
struct MsrpSessionInfo
{
    // Identity
    QString sessionKey;       // internal unique key (see MsrpSessionStore)
    QString sipCallId;
    QString sipDialogId;
    // Task W101 (Live MSRP SIP Integration, Phase 4): the real SIP Call-ID
    // header value (pj::CallInfo::callIdString) — distinct from sipCallId
    // above, which is this app's own internal call identifier
    // ("pjsip-<n>"), and distinct from sipDialogId (reserved for a future
    // local-tag/remote-tag pair once exposed by a public pjsua2/pjsua
    // API). Never derived from IP/port/peer-URI alone, per task rule 4.
    QString sipHeaderCallId;
    // Index of this session's m=message section within the negotiated SDP
    // — part of the SIP<->MSRP dialog mapping (media index), so a call
    // with multiple simultaneous MSRP sessions (or renegotiated media
    // order) can still be disambiguated without relying on IP/port alone.
    int mediaIndex{-1};
    QString localSessionId;
    QString remoteSessionId;
    QStringList localPath;    // list of msrp(s):// URIs, this end first
    QStringList remotePath;

    // Negotiation
    MsrpTransportProtocol localTransport{MsrpTransportProtocol::Unknown};
    MsrpTransportProtocol remoteTransport{MsrpTransportProtocol::Unknown};
    MsrpSetup localSetup{MsrpSetup::Unknown};
    MsrpSetup remoteSetup{MsrpSetup::Unknown};
    QString connectionMode;   // a=connection value, verbatim ("new"/"existing")
    MsrpDirection localDirection{MsrpDirection::Unknown};
    MsrpDirection remoteDirection{MsrpDirection::Unknown};
    QStringList acceptTypes;
    QStringList acceptWrappedTypes;
    QString fileSelector;
    QString fileDisposition;
    QString offerAnswerState;  // "offer" / "answer" / "renegotiation" / ...

    // Lifecycle
    MsrpSessionState state{MsrpSessionState::Disabled};
    MsrpRole role{MsrpRole::Unknown};

    // Diagnostic
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime connectedAt;
    QDateTime closedAt;
    MsrpParseStatus parseStatus{MsrpParseStatus::Ok};
    QStringList warnings;
    QString lastError;
    quint64 bytesSent{0};
    quint64 bytesReceived{0};
    quint64 framesSent{0};
    quint64 framesReceived{0};
    quint64 messagesCompleted{0};
    int transactionsPending{0};

    bool isEstablished() const { return state == MsrpSessionState::Established; }
};

Q_DECLARE_METATYPE(MsrpSessionInfo)
