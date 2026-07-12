#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include "msrp/MsrpTypes.h"

// One MSRP diagnostic event (Task W100, section Q) — SEND/REPORT/response/
// TCP-connect/TLS-handshake/abort/close, feeding the MSRP page's frame log
// and the Interop JSON export's msrpEvents array. Raw frame bytes are
// always redacted/bounded before being stored here — never the full
// unredacted wire bytes of an arbitrarily large body.
struct MsrpDiagnosticsEvent
{
    enum class Direction { Outbound, Inbound };
    enum class Kind { TcpConnect, TcpAccept, TlsHandshake, Frame, Abort, Close, Error };

    QDateTime timestamp;
    Direction direction{Direction::Outbound};
    Kind kind{Kind::Frame};

    QString sessionKey;
    QString transactionId;
    QString messageId;
    QString method;          // "SEND" / "REPORT" / "response" / "" for transport-level events
    int responseCode{0};
    QString statusHeader;
    QString toPathRedacted;
    QString fromPathRedacted;
    QString contentType;
    QString byteRangeText;
    MsrpContinuation continuation{MsrpContinuation::Complete};
    QString bodyPreview;     // safe, size-capped preview — never the raw body
    qint64 bodyLength{0};
    QString rawFrameRedacted; // size-capped, path-redacted approximation of the wire frame
    MsrpTransportProtocol transport{MsrpTransportProtocol::Unknown};
    MsrpParseStatus parseStatus{MsrpParseStatus::Ok};
    QStringList warnings;
    QString error;
    QString localEndpointRedacted;
    QString remoteEndpointRedacted;
};

Q_DECLARE_METATYPE(MsrpDiagnosticsEvent)
