#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QString>

#include "sip/PresenceInfo.h"
#include "sip/SipMessageTrace.h"

// One row of the Presence diagnostics feed: a SUBSCRIBE/NOTIFY request or
// response (plus whatever PIDF presence info could be parsed from its
// body). Built directly from the same raw SIP capture (SipTraceLogger) used
// by the SIP Ladder — read-only, never starts/represents a real
// subscription by itself, and never mixed into MessagingTraceEntry/
// MessageHistoryEntry.
struct PresenceTraceEntry
{
    QDateTime                   timestamp;
    SipMessageTrace::Direction  direction{SipMessageTrace::Direction::Outbound};
    QString                     method;      // SUBSCRIBE or NOTIFY
    int                         statusCode{0};
    QString                     statusText;
    QString                     fromUri;
    QString                     toUri;
    QString                     callId;
    QString                     cSeq;
    QString                     eventPackage;      // Event header value, e.g. "presence"
    QString                     subscriptionState; // Subscription-State header, state token only
    int                         subscriptionExpires{-1}; // -1 = not present
    QString                     subscriptionReason;      // normalized reason= param
    QString                     contentType;
    QString                     rawSip;

    PresenceInfo pidf; // parsed via PidfParser when contentType is application/pidf+xml
};

Q_DECLARE_METATYPE(PresenceTraceEntry)
