#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "sip/CpimInfo.h"
#include "sip/ImdnInfo.h"
#include "sip/IsComposingInfo.h"
#include "sip/MessagingContentKind.h"
#include "sip/SdpMsrpInfo.h"
#include "sip/SipMessageTrace.h"

// One row of the Messaging Diagnostics feed: a SIP MESSAGE (or a SIP
// request/response whose body is messaging-related, e.g. an INVITE/re-INVITE
// carrying an SDP m=message MSRP offer) plus whatever structured
// CPIM/IMDN/is-composing/MSRP-SDP information could be detected from its
// body. Read-only — building an entry never starts a real MSRP session.
struct MessagingTraceEntry
{
    QDateTime               timestamp;
    SipMessageTrace::Direction direction{SipMessageTrace::Direction::Outbound};
    QString                  method;      // MESSAGE, INVITE, etc.
    int                      statusCode{0};
    QString                  statusText;
    QString                  fromUri;
    QString                  toUri;
    QString                  callId;
    QString                  cSeq;
    QString                  contentType;
    QString                  contentEncoding; // raw Content-Encoding header value, e.g. "deflate"; empty if none
    QString                  bodyPreview; // truncated, single-line, safe-for-UI preview of the body
                                          // (decoded content when contentEncoding was successfully reversed)
    QString                  decodedBodyPreview; // same as bodyPreview when decoding applies/succeeds;
                                                  // empty when there was nothing to decode or decoding failed
    QString                  rawSip;      // full raw SIP text; Authorization headers already
                                          // redacted by SipTraceLogger before this entry was built.
                                          // Always the original (still-encoded) bytes — never rewritten.

    bool                     contentEncodingDecodeFailed{false}; // true if contentEncoding was recognized
                                                                  // but decoding the body against it failed

    MessagingContentKind contentKind{MessagingContentKind::Unknown};

    CpimInfo         cpim;
    ImdnInfo         imdn;
    IsComposingInfo  isComposing;
    SdpMsrpInfo      sdpMsrp;

    QString summary() const
    {
        if (statusCode > 0)
            return QStringLiteral("%1 %2").arg(statusCode).arg(statusText);
        return method;
    }
};

Q_DECLARE_METATYPE(MessagingTraceEntry)
