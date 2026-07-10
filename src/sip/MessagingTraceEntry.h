#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "sip/CpimInfo.h"
#include "sip/DeflateDecoder.h"
#include "sip/ImdnInfo.h"
#include "sip/IsComposingInfo.h"
#include "sip/MessagingContentKind.h"
#include "sip/RcsFtHttpInfo.h"
#include "sip/SdpMsrpInfo.h"
#include "sip/SipMessageTrace.h"

// Content-Encoding decode outcome for a messaging body, as required by
// Task W095's export schema (contentEncoding/decodeStatus/decodeVariant/...).
enum class ContentDecodeStatus { NotNeeded, Decoded, Failed, Unsupported, LimitExceeded };

inline QString contentDecodeStatusToString(ContentDecodeStatus status)
{
    switch (status) {
    case ContentDecodeStatus::NotNeeded:     return QStringLiteral("not-needed");
    case ContentDecodeStatus::Decoded:       return QStringLiteral("decoded");
    case ContentDecodeStatus::Failed:        return QStringLiteral("failed");
    case ContentDecodeStatus::Unsupported:   return QStringLiteral("unsupported");
    case ContentDecodeStatus::LimitExceeded: return QStringLiteral("limit-exceeded");
    }
    return QStringLiteral("not-needed");
}

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
    QString                  bodyPreview; // truncated, single-line, safe-for-UI preview of the
                                          // *decoded* body (never raw compressed bytes)
    QString                  rawSip;      // full raw SIP text; Authorization headers already
                                          // redacted by SipTraceLogger before this entry was built

    MessagingContentKind contentKind{MessagingContentKind::Unknown};

    // Content-Encoding diagnostics (Task W095). decodeStatus is NotNeeded
    // whenever contentEncoding is empty (the overwhelming majority of
    // traces); the CPIM/IMDN/is-composing/RCS parsers below only ever see
    // the decoded body — never raw compressed bytes.
    QString              contentEncoding;
    ContentDecodeStatus  decodeStatus{ContentDecodeStatus::NotNeeded};
    DeflateDecoder::Variant decodeVariant{DeflateDecoder::Variant::None};
    qint64               compressedBodyLength{0};
    qint64               decodedBodyLength{0};
    QString              decodeError;         // only set when decodeStatus != NotNeeded/Decoded
    QString              decodedBodyPreview;  // truncated preview of the decoded body, when decoded

    CpimInfo         cpim;
    ImdnInfo         imdn;
    IsComposingInfo  isComposing;
    SdpMsrpInfo      sdpMsrp;
    RcsFtHttpInfo    rcsFtHttp;

    QString summary() const
    {
        if (statusCode > 0)
            return QStringLiteral("%1 %2").arg(statusCode).arg(statusText);
        return method;
    }
};

Q_DECLARE_METATYPE(MessagingTraceEntry)
