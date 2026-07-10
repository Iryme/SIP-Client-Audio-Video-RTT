#pragma once
#include <QDateTime>
#include <QString>

#include "sip/ImdnInfo.h"

// Builds RFC 5438 IMDN (message/imdn+xml) document bodies. Pure Qt/text —
// no PJSIP or network dependency — so it is fully unit-testable. This
// client only ever *generates* delivered/displayed/failed/error reports
// (never forbidden/processed, which are inbound-only extension statuses;
// see ImdnInfo::Disposition).
class ImdnGenerator
{
public:
    // messageId: the Message-ID header value of the message being reported
    // on (RFC 5438 §5.2 <message-id>). datetime defaults to "now" (UTC) when
    // left null. originalRecipient/finalRecipient are optional (RFC 5438
    // §5.2 <original-recipient-uri>/<final-recipient-uri>) and omitted from
    // the document when empty.
    static QString generate(const QString &messageId,
                            ImdnInfo::Disposition disposition,
                            const QString &originalRecipient = QString(),
                            const QString &finalRecipient = QString(),
                            const QDateTime &datetime = QDateTime());
};
