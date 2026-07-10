#include "MessagingMessageDetailsDialog.h"

#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

#include "sip/DeflateDecoder.h"
#include "sip/MessagingContentKind.h"
#include "sip/SipBodyExtractor.h"
#include "sip/UrlRedactor.h"

namespace {

QString normalizeLineEndings(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

// Body shown to the operator: for a successfully decoded (or never-encoded)
// entry this is the decoded/plain text; a failed/unsupported/limit-exceeded
// Content-Encoding never falls back to showing the raw compressed bytes —
// see the "Content-Encoding" section built below for that case instead.
QString extractDisplayBody(const MessagingTraceEntry &entry)
{
    if (!entry.contentEncoding.isEmpty() && entry.decodeStatus != ContentDecodeStatus::Decoded)
        return QString();
    const SipBodyExtractor::Result extraction = SipBodyExtractor::extract(entry.rawSip);
    return normalizeLineEndings(QString::fromUtf8(extraction.rawBodyBytes)).trimmed();
}

} // namespace

MessagingMessageDetailsDialog::MessagingMessageDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Messaging Diagnostics — Message Details"));
    setMinimumSize(760, 620);
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::Tool, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_title = new QLabel(tr("Messaging trace"), this);
    m_title->setStyleSheet("font-size: 16px; font-weight: 600;");
    root->addWidget(m_title);

    auto *meta = new QGridLayout();
    meta->setHorizontalSpacing(12);
    meta->setVerticalSpacing(6);

    auto makeValue = [this](const QString &placeholder) -> QLabel* {
        auto *label = new QLabel(placeholder, this);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        return label;
    };

    meta->addWidget(new QLabel(tr("Timestamp:"), this), 0, 0);
    m_timestamp = makeValue(QStringLiteral("—"));
    meta->addWidget(m_timestamp, 0, 1);

    meta->addWidget(new QLabel(tr("Direction:"), this), 0, 2);
    m_direction = makeValue(QStringLiteral("—"));
    meta->addWidget(m_direction, 0, 3);

    meta->addWidget(new QLabel(tr("Method / Status:"), this), 1, 0);
    m_method = makeValue(QStringLiteral("—"));
    meta->addWidget(m_method, 1, 1);

    meta->addWidget(new QLabel(tr("Content-Type / Kind:"), this), 1, 2);
    m_contentKind = makeValue(QStringLiteral("—"));
    meta->addWidget(m_contentKind, 1, 3);

    meta->addWidget(new QLabel(tr("From:"), this), 2, 0);
    m_from = makeValue(QStringLiteral("—"));
    meta->addWidget(m_from, 2, 1, 1, 3);

    meta->addWidget(new QLabel(tr("To:"), this), 3, 0);
    m_to = makeValue(QStringLiteral("—"));
    meta->addWidget(m_to, 3, 1, 1, 3);

    meta->addWidget(new QLabel(tr("Call-ID:"), this), 4, 0);
    m_callId = makeValue(QStringLiteral("—"));
    meta->addWidget(m_callId, 4, 1, 1, 3);

    meta->addWidget(new QLabel(tr("CSeq:"), this), 5, 0);
    m_cseq = makeValue(QStringLiteral("—"));
    meta->addWidget(m_cseq, 5, 1, 1, 3);

    meta->addWidget(new QLabel(tr("Content-Type:"), this), 6, 0);
    m_contentType = makeValue(QStringLiteral("—"));
    meta->addWidget(m_contentType, 6, 1, 1, 3);

    root->addLayout(meta);

    auto sectionHeader = [](const QString &title) {
        auto *label = new QLabel(title);
        label->setStyleSheet("font-weight: 600; margin-top: 4px;");
        return label;
    };

    root->addWidget(sectionHeader(tr("Structured diagnostics (CPIM / IMDN / is-composing / MSRP-SDP)")));
    m_structured = new QPlainTextEdit(this);
    m_structured->setReadOnly(true);
    m_structured->setMinimumHeight(120);
    m_structured->setPlaceholderText(tr("No structured diagnostics detected for this message"));
    root->addWidget(m_structured, 1);

    root->addWidget(sectionHeader(tr("Body")));
    m_body = new QPlainTextEdit(this);
    m_body->setReadOnly(true);
    m_body->setMinimumHeight(90);
    m_body->setPlaceholderText(tr("Body not available"));
    root->addWidget(m_body, 1);

    root->addWidget(sectionHeader(tr("Raw SIP (redacted)")));
    m_raw = new QPlainTextEdit(this);
    m_raw->setReadOnly(true);
    m_raw->setMinimumHeight(140);
    m_raw->setPlaceholderText(tr("Raw SIP not available"));
    root->addWidget(m_raw, 2);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    m_copyRaw = new QPushButton(tr("Copy raw SIP"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    buttons->addWidget(m_copyRaw);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(m_copyRaw, &QPushButton::clicked, this, &MessagingMessageDetailsDialog::onCopyRawClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void MessagingMessageDetailsDialog::setEntry(const MessagingTraceEntry &entry)
{
    m_entry = entry;

    const QString summary = entry.summary();
    m_title->setText(summary.isEmpty() ? tr("Messaging trace") : summary);
    m_timestamp->setText(entry.timestamp.isValid()
        ? entry.timestamp.toString(Qt::ISODateWithMs)
        : tr("Unknown"));
    m_direction->setText(formatDirection(entry.direction));
    m_method->setText(entry.statusCode > 0
        ? QStringLiteral("%1 %2").arg(entry.statusCode).arg(entry.statusText)
        : entry.method);
    m_from->setText(entry.fromUri.isEmpty() ? tr("Not available") : entry.fromUri);
    m_to->setText(entry.toUri.isEmpty() ? tr("Not available") : entry.toUri);
    m_callId->setText(entry.callId.isEmpty() ? tr("Not available") : entry.callId);
    m_cseq->setText(entry.cSeq.isEmpty() ? tr("Not available") : entry.cSeq);
    m_contentType->setText(entry.contentType.isEmpty() ? tr("Not available") : entry.contentType);
    m_contentKind->setText(MessagingContentKindDetector::toString(entry.contentKind));

    const QString structured = formatStructuredSections(entry);
    m_structured->setPlainText(structured);

    const QString body = extractDisplayBody(entry);
    m_body->setPlainText(body.isEmpty() ? tr("No body present") : body);

    if (entry.rawSip.isEmpty()) {
        m_raw->setPlainText(tr("Raw SIP not available"));
        m_copyRaw->setEnabled(false);
    } else {
        m_raw->setPlainText(entry.rawSip);
        m_copyRaw->setEnabled(true);
    }
}

void MessagingMessageDetailsDialog::onCopyRawClicked()
{
    if (m_entry.rawSip.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_entry.rawSip);
}

QString MessagingMessageDetailsDialog::formatDirection(SipMessageTrace::Direction direction)
{
    return direction == SipMessageTrace::Direction::Outbound
        ? QObject::tr("Outbound")
        : QObject::tr("Inbound");
}

QString MessagingMessageDetailsDialog::formatStructuredSections(const MessagingTraceEntry &entry)
{
    QString out;
    QTextStream ts(&out);

    if (!entry.contentEncoding.isEmpty()) {
        ts << "Content-Encoding\n";
        ts << "  Encoding: " << entry.contentEncoding << '\n';
        ts << "  Decode status: " << contentDecodeStatusToString(entry.decodeStatus) << '\n';
        ts << "  Decode variant: " << DeflateDecoder::variantToString(entry.decodeVariant) << '\n';
        ts << "  Compressed size: " << entry.compressedBodyLength << " bytes\n";
        ts << "  Decoded size: " << entry.decodedBodyLength << " bytes\n";
        if (!entry.decodeError.isEmpty())
            ts << "  Error: " << entry.decodeError << '\n';
        ts << '\n';
    }

    if (entry.rcsFtHttp.present) {
        ts << "RCS FT HTTP (read-only — file is never downloaded or opened by this client)\n";
        ts << "  file-info type: " << (entry.rcsFtHttp.fileInfoType.isEmpty() ? QStringLiteral("Not available") : entry.rcsFtHttp.fileInfoType) << '\n';
        ts << "  File name: " << (entry.rcsFtHttp.fileName.isEmpty() ? QStringLiteral("Not available") : entry.rcsFtHttp.fileName) << '\n';
        ts << "  Content-Type: " << (entry.rcsFtHttp.contentType.isEmpty() ? QStringLiteral("Not available") : entry.rcsFtHttp.contentType) << '\n';
        ts << "  File size: " << (entry.rcsFtHttp.fileSize >= 0 ? QString::number(entry.rcsFtHttp.fileSize) + QStringLiteral(" bytes") : QStringLiteral("Not available")) << '\n';
        ts << "  Expires: " << (entry.rcsFtHttp.expiresAt.isEmpty() ? QStringLiteral("Not available") : entry.rcsFtHttp.expiresAt) << '\n';
        ts << "  Thumbnail present: " << (entry.rcsFtHttp.thumbnailPresent ? QStringLiteral("yes") : QStringLiteral("no")) << '\n';
        ts << "  URL (redacted): " << UrlRedactor::redact(entry.rcsFtHttp.dataUrl) << '\n';
        ts << '\n';
    }

    if (entry.cpim.present) {
        ts << "CPIM\n";
        ts << "  From: " << (entry.cpim.from.isEmpty() ? QStringLiteral("Not available") : entry.cpim.from) << '\n';
        ts << "  To: " << (entry.cpim.to.isEmpty() ? QStringLiteral("Not available") : entry.cpim.to) << '\n';
        ts << "  DateTime: " << (entry.cpim.dateTime.isEmpty() ? QStringLiteral("Not available") : entry.cpim.dateTime) << '\n';
        ts << "  Subject: " << (entry.cpim.subject.isEmpty() ? QStringLiteral("Not available") : entry.cpim.subject) << '\n';
        ts << "  Content-Type: " << (entry.cpim.contentType.isEmpty() ? QStringLiteral("Not available") : entry.cpim.contentType) << '\n';
        ts << '\n';
    }

    if (entry.imdn.present) {
        ts << "IMDN\n";
        ts << "  Disposition: " << ImdnInfo::dispositionToString(entry.imdn.disposition) << '\n';
        ts << "  Message-ID: " << (entry.imdn.messageId.isEmpty() ? QStringLiteral("Not available") : entry.imdn.messageId) << '\n';
        ts << "  original-recipient: " << (entry.imdn.originalRecipient.isEmpty() ? QStringLiteral("Not available") : entry.imdn.originalRecipient) << '\n';
        ts << "  final-recipient: " << (entry.imdn.finalRecipient.isEmpty() ? QStringLiteral("Not available") : entry.imdn.finalRecipient) << '\n';
        ts << '\n';
    }

    if (entry.isComposing.present) {
        ts << "is-composing\n";
        ts << "  State: " << IsComposingInfo::stateToString(entry.isComposing.state) << '\n';
        ts << "  Timeout: " << (entry.isComposing.timeout.isEmpty() ? QStringLiteral("Not available") : entry.isComposing.timeout) << '\n';
        ts << "  Refresh: " << (entry.isComposing.refresh.isEmpty() ? QStringLiteral("Not available") : entry.isComposing.refresh) << '\n';
        ts << '\n';
    }

    if (entry.sdpMsrp.present) {
        ts << "SDP MSRP (diagnostic only — no session started)\n";
        ts << "  Media line: " << entry.sdpMsrp.mediaLine << '\n';
        ts << "  Transport: " << (entry.sdpMsrp.transportProtocol.isEmpty() ? QStringLiteral("Not available") : entry.sdpMsrp.transportProtocol) << '\n';
        ts << "  a=path: " << (entry.sdpMsrp.path.isEmpty() ? QStringLiteral("Not available") : entry.sdpMsrp.path) << '\n';
        ts << "  a=accept-types: " << (entry.sdpMsrp.acceptTypes.isEmpty() ? QStringLiteral("Not available") : entry.sdpMsrp.acceptTypes) << '\n';
        ts << "  a=setup: " << (entry.sdpMsrp.setup.isEmpty() ? QStringLiteral("Not available") : entry.sdpMsrp.setup) << '\n';
        ts << "  a=connection: " << (entry.sdpMsrp.connection.isEmpty() ? QStringLiteral("Not available") : entry.sdpMsrp.connection) << '\n';
        ts << "  Session-ID: " << (entry.sdpMsrp.sessionId.isEmpty() ? QStringLiteral("Not available") : entry.sdpMsrp.sessionId) << '\n';
        ts << '\n';
    }

    return out;
}
