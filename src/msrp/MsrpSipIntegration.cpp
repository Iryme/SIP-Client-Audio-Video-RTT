#include "MsrpSipIntegration.h"

#include "msrp/MsrpSdpNegotiator.h"
#include "msrp/MsrpSessionStore.h"

namespace MsrpSipIntegration {

void detectFromSdp(const QString &sdpText, const QString &sipCallId)
{
    if (sdpText.isEmpty() || sipCallId.isEmpty())
        return;

    const auto blocks = MsrpSdpNegotiator::parseMessageBlocks(sdpText);
    if (blocks.isEmpty())
        return;

    const auto &block = blocks.first();
    const QString sessionKey = QStringLiteral("sip-call:%1").arg(sipCallId);

    MsrpSessionInfo info = MsrpSessionStore::instance().get(sessionKey);
    info.sessionKey = sessionKey;
    info.sipCallId = sipCallId;
    info.localTransport = block.transport;
    info.localSetup = block.setup;
    info.acceptTypes = block.acceptTypes;
    info.acceptWrappedTypes = block.acceptWrappedTypes;
    info.fileSelector = block.fileSelector;
    info.fileDisposition = block.fileDisposition;
    info.parseStatus = block.parseStatus;
    info.warnings = block.warnings;

    if (!block.path.isEmpty()) {
        QStringList pathStrings;
        for (const auto &u : block.path)
            pathStrings << u.toString();
        info.remotePath = pathStrings;
    }

    // Deliberately never advances past "Detected" here — this function only
    // ever observes already-created/received SDP text; it never drives a
    // transport connection, so "negotiated"/"connected"/"established" are
    // never set by this path (see header comment: no live SDP injection).
    if (info.state == MsrpSessionState::Disabled)
        info.state = MsrpSessionState::Detected;

    MsrpSessionStore::instance().upsert(info);
}

} // namespace MsrpSipIntegration
