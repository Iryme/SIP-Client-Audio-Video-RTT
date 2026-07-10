#pragma once
#include <QList>
#include <QPair>
#include <QString>

#include "sip/ImdnInfo.h"
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

    // RFC 5438 Message-ID header value (Task W096). Set whenever
    // imdnRequested is true (correlation only matters when we asked for a
    // disposition notification) or when this ComposedSipMessage IS an IMDN
    // report itself (see composeImdnReport()). Empty otherwise.
    QString messageId;

    // Task W096: set only when this ComposedSipMessage is an IMDN report
    // (message/imdn+xml) built by composeImdnReport(). correlatedMessageId
    // is the Message-ID of the original message this report is about.
    bool    isImdnReport{false};
    QString correlatedMessageId;

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

    // Task W096: builds an outbound RFC 5438 IMDN report (delivered/
    // displayed/failed/error) — a message/imdn+xml body, never CPIM-wrapped,
    // never requesting a further IMDN of its own. Pure builder like
    // compose(); sending is a separate step (SipManager::sendSipMessage).
    struct ImdnReportOptions
    {
        QString toUri;
        QString fromUri;
        QString originalMessageId; // the Message-ID being reported on
        ImdnInfo::Disposition disposition{ImdnInfo::Disposition::None};
    };
    static ComposedSipMessage composeImdnReport(const ImdnReportOptions &opts);
};
