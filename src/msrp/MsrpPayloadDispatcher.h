#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

#include "sip/MessagingContentKind.h"

// Dispatches a completed MSRP message payload (Task W100, section L) to the
// existing messaging infrastructure — never re-implements CPIM/IMDN/
// is-composing parsing. Mirrors SipManager::onAccountInstantMessageReceived's
// dispatch logic exactly, just fed from MsrpSession::payloadReceived instead
// of a SIP MESSAGE callback.
namespace MsrpPayloadDispatcher {

struct DispatchResult
{
    MessagingContentKind kind{MessagingContentKind::Unknown};
    bool addedToHistory{false};
    QString warning;
};

// contentType/body come directly from the assembled MSRP message (already
// chunk-reassembled). fromUri/toUri/callId/profileId identify the
// conversation for MessageHistoryStore, same fields SIP MESSAGE uses.
DispatchResult dispatch(const QString &contentType, const QByteArray &body,
                        const QString &fromUri, const QString &toUri,
                        const QString &callId, const QString &profileId);

} // namespace MsrpPayloadDispatcher
