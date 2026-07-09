#pragma once
#include <QList>
#include <QPair>
#include <QString>

#include "sip/MessagingContentKind.h"

// Result of composing an outbound SIP MESSAGE. Pure data — building a
// ComposedSipMessage never touches the network or PJSIP; it only decides
// what bytes *would* be sent, so it is fully unit-testable without a live
// SIP stack. Sending it is a separate step (SipManager::sendSipMessage).
struct ComposedSipMessage
{
    bool    valid{false};
    QString error; // human-readable reason when valid == false

    QString toUri;       // normalized recipient (SipUriNormalizer output)
    QString fromUri;
    QString contentType; // outer Content-Type header value actually sent on the wire
    QString body;        // final UTF-8 body (CPIM-wrapped when contentType is message/cpim)
    QString rawSip;       // synthetic raw SIP text (request line + headers + blank line + body),
                           // fed into SipTraceLogger so it flows through the existing
                           // MessagingDiagnosticsStore/MessagingEventStore pipeline unchanged
    QString callId;
    QString cSeq;
    bool    imdnRequested{false};

    // Extra SIP headers to inject on the wire (e.g. IMDN request headers).
    // Also reflected inside rawSip for the synthetic diagnostic trace.
    QList<QPair<QString, QString>> extraHeaders;
};

// Builds outbound SIP MESSAGE requests (plain/html/CPIM-wrapped body, optional
// IMDN request headers) without any PJSIP or network dependency. No parsing
// logic is duplicated here — CPIM wrapping is delegated to CpimBuilder, and
// the composed rawSip is later parsed back into a MessagingTraceEntry by the
// existing, unmodified MessagingDiagnosticsStore::buildEntry() (Task W090).
class SipMessageComposer
{
public:
    struct Options
    {
        QString toUri;   // user-entered recipient; validated via SipUriNormalizer
        QString fromUri; // sender URI, taken from the active SIP profile
        MessagingContentKind contentType{MessagingContentKind::PlainText}; // Plain, Html, or Cpim only
        QString body;    // user-authored plain-text message
        bool    cpimEnabled{false};   // gates contentType == Cpim (AppSettings::enableCpim)
        bool    requestImdn{false};   // adds IMDN-request headers only; does not generate IMDN itself
    };

    static ComposedSipMessage compose(const Options &opts);
};
