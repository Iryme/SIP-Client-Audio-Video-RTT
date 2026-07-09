#include "SipMessageComposer.h"

#include <QDateTime>
#include <QUuid>

#include "sip/CpimBuilder.h"
#include "sip/SipUriNormalizer.h"

namespace {

QString outerContentTypeFor(MessagingContentKind kind)
{
    switch (kind) {
    case MessagingContentKind::Html: return QStringLiteral("text/html; charset=utf-8");
    case MessagingContentKind::Cpim: return QStringLiteral("message/cpim");
    case MessagingContentKind::PlainText:
    default:
        return QStringLiteral("text/plain; charset=utf-8");
    }
}

QString newToken()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

ComposedSipMessage SipMessageComposer::compose(const Options &opts)
{
    ComposedSipMessage out;

    if (opts.contentType != MessagingContentKind::PlainText
        && opts.contentType != MessagingContentKind::Html
        && opts.contentType != MessagingContentKind::Cpim) {
        out.error = QStringLiteral("Unsupported Content-Type for SIP MESSAGE composition");
        return out;
    }

    const SipUriNormalizer::Result dest = SipUriNormalizer::normalize(opts.toUri);
    if (!dest.isValid) {
        out.error = dest.error.isEmpty()
            ? QStringLiteral("Invalid or empty destination URI")
            : dest.error;
        return out;
    }

    if (opts.body.trimmed().isEmpty()) {
        out.error = QStringLiteral("Message body is empty");
        return out;
    }

    if (opts.contentType == MessagingContentKind::Cpim && !opts.cpimEnabled) {
        out.error = QStringLiteral("CPIM is not enabled (Settings > Messaging > Enable CPIM)");
        return out;
    }

    out.toUri         = dest.uri;
    out.fromUri       = opts.fromUri;
    out.callId        = newToken();
    out.cSeq          = QStringLiteral("1 MESSAGE");
    out.imdnRequested = opts.requestImdn;

    if (opts.contentType == MessagingContentKind::Cpim) {
        out.contentType = outerContentTypeFor(MessagingContentKind::Cpim);
        out.body = CpimBuilder::build(opts.fromUri, out.toUri,
                                       QStringLiteral("text/plain; charset=utf-8"),
                                       opts.body);
    } else {
        out.contentType = outerContentTypeFor(opts.contentType);
        out.body = opts.body;
    }

    if (opts.requestImdn) {
        out.extraHeaders.append({QStringLiteral("Message-ID"), newToken()});
        out.extraHeaders.append({QStringLiteral("Disposition-Notification"),
                                  QStringLiteral("positive-delivery, positive-display")});
    }

    // Synthetic raw SIP text for the diagnostics pipeline — mirrors the
    // per-action summary style SipManager::makeCall() already emits for
    // outbound INVITEs, so MessagingDiagnosticsStore::buildEntry() (Task
    // W090, unmodified) can extract and parse the body exactly as it would
    // for a captured wire message.
    QString raw;
    raw += QStringLiteral("MESSAGE %1 SIP/2.0\n").arg(out.toUri);
    if (!out.fromUri.isEmpty())
        raw += QStringLiteral("From: %1\n").arg(out.fromUri);
    raw += QStringLiteral("To: %1\n").arg(out.toUri);
    raw += QStringLiteral("Call-ID: %1\n").arg(out.callId);
    raw += QStringLiteral("CSeq: %1\n").arg(out.cSeq);
    raw += QStringLiteral("Content-Type: %1\n").arg(out.contentType);
    for (const auto &hdr : out.extraHeaders)
        raw += QStringLiteral("%1: %2\n").arg(hdr.first, hdr.second);
    raw += QStringLiteral("Content-Length: %1\n").arg(out.body.toUtf8().size());
    raw += QStringLiteral("\n");
    raw += out.body;
    out.rawSip = raw;

    out.valid = true;
    return out;
}
