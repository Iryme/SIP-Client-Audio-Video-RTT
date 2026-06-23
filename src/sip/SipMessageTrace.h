#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

struct SipMessageTrace
{
    enum class Direction { Inbound, Outbound };

    QDateTime  timestamp;
    Direction  direction{Direction::Outbound};
    QString    method;        // SIP method: REGISTER, INVITE, BYE, ACK, CANCEL, etc.
    int        statusCode{0}; // 0 for requests; 1xx-6xx for responses
    QString    statusText;    // "OK", "Trying", "Ringing", "Unauthorized", etc.
    QString    fromUri;
    QString    toUri;
    QString    callId;
    QString    cSeq;
    QString    rawSip;        // Stored only when Raw log level is enabled;
                              // Authorization headers are replaced with [REDACTED].

    QString summary() const
    {
        if (statusCode > 0)
            return QStringLiteral("%1 %2").arg(statusCode).arg(statusText);
        return method;
    }
};

Q_DECLARE_METATYPE(SipMessageTrace)
