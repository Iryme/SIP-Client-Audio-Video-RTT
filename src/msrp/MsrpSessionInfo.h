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
