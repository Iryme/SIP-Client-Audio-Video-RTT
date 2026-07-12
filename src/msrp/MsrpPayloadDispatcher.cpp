#include "MsrpPayloadDispatcher.h"

#include "sip/CpimParser.h"
#include "sip/ImdnParser.h"
#include "sip/IsComposingParser.h"
#include "sip/MessageHistoryStore.h"

namespace MsrpPayloadDispatcher {

namespace {

bool looksLikeText(const QByteArray &body)
{
    // Conservative binary sniff: reject if it contains a NUL byte or an
    // excessive proportion of non-printable bytes — good enough to avoid
    // rendering genuinely binary content as text (task requirement L).
    if (body.contains('\0'))
        return false;
    int nonPrintable = 0;
    for (unsigned char c : body) {
        if (c < 0x09 || (c > 0x0d && c < 0x20))
            ++nonPrintable;
    }
    return body.isEmpty() || (nonPrintable * 20 < body.size());
}

DispatchResult dispatchInner(MessagingContentKind kind, const QString &contentType,
                             const QByteArray &body, const QString &fromUri,
                             const QString &toUri, const QString &callId,
                             const QString &profileId)
{
    DispatchResult result;
    result.kind = kind;
    const QString text = QString::fromUtf8(body);

    switch (kind) {
    case MessagingContentKind::Imdn: {
        const ImdnInfo info = ImdnParser::parse(text);
        MessageHistoryStore::instance().appendInboundImdn(
            fromUri, toUri, fromUri, text, callId, profileId, info.messageId);
        if (info.present && !info.messageId.isEmpty()) {
            MessageHistoryEntry::DeliveryState state = MessageHistoryEntry::DeliveryState::None;
            switch (info.disposition) {
            case ImdnInfo::Disposition::Delivered: state = MessageHistoryEntry::DeliveryState::Delivered; break;
            case ImdnInfo::Disposition::Displayed: state = MessageHistoryEntry::DeliveryState::Displayed; break;
            case ImdnInfo::Disposition::Failed:
            case ImdnInfo::Disposition::Forbidden:
                state = MessageHistoryEntry::DeliveryState::Failed; break;
            case ImdnInfo::Disposition::Error: state = MessageHistoryEntry::DeliveryState::Error; break;
            default: break;
            }
            if (state != MessageHistoryEntry::DeliveryState::None)
                MessageHistoryStore::instance().correlateDelivery(info.messageId, state);
        }
        result.addedToHistory = true;
        return result;
    }
    case MessagingContentKind::IsComposing: {
        const IsComposingInfo info = IsComposingParser::parse(text);
        MessageHistoryStore::instance().appendInboundTyping(
            fromUri, toUri, fromUri, text, callId, profileId,
            IsComposingInfo::stateToString(info.state));
        result.addedToHistory = true;
        return result;
    }
    case MessagingContentKind::PlainText:
    case MessagingContentKind::Html:
        MessageHistoryStore::instance().appendInbound(
            fromUri, toUri, fromUri, contentType, text, callId, profileId);
        result.addedToHistory = true;
        return result;
    default:
        break;
    }

    // Unknown Content-Type: only add to history if it plausibly is text;
    // otherwise keep it diagnostics-only (never render a binary blob as
    // text), per task requirement L.
    if (looksLikeText(body)) {
        MessageHistoryStore::instance().appendInbound(
            fromUri, toUri, fromUri, contentType, text, callId, profileId);
        result.addedToHistory = true;
    } else {
        result.warning = QStringLiteral("binary/unrecognized Content-Type '%1' — kept in diagnostics only")
            .arg(contentType);
    }
    return result;
}

} // namespace

DispatchResult dispatch(const QString &contentType, const QByteArray &body,
                        const QString &fromUri, const QString &toUri,
                        const QString &callId, const QString &profileId)
{
    const MessagingContentKind kind = MessagingContentKindDetector::detect(contentType);

    if (kind == MessagingContentKind::Cpim) {
        const CpimInfo cpim = CpimParser::parse(QString::fromUtf8(body));
        if (cpim.present && !cpim.contentType.isEmpty()) {
            const MessagingContentKind innerKind = MessagingContentKindDetector::detect(cpim.contentType);
            return dispatchInner(innerKind, cpim.contentType, cpim.wrappedBody.toUtf8(),
                                 fromUri, toUri, callId, profileId);
        }
        // CPIM present but couldn't be unwrapped: fall through as plain text
        // of the raw CPIM body so it is at least visible, not dropped.
        return dispatchInner(MessagingContentKind::PlainText, QStringLiteral("message/cpim"),
                             body, fromUri, toUri, callId, profileId);
    }

    return dispatchInner(kind, contentType, body, fromUri, toUri, callId, profileId);
}

} // namespace MsrpPayloadDispatcher
