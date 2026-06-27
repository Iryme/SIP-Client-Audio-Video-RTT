#include "SipMessageDetailsDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
static QString normalizeLineEndings(QString text)
{
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    return text;
}
}

SipMessageDetailsDialog::SipMessageDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SIP Message Details"));
    setMinimumSize(760, 560);
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::Tool, true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_title = new QLabel(tr("SIP Message"), this);
    m_title->setStyleSheet("font-size: 16px; font-weight: 600;");
    root->addWidget(m_title);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet("color: #b7c4d6;");
    root->addWidget(m_summary);

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

    root->addLayout(meta);

    auto sectionHeader = [](const QString &title) {
        auto *label = new QLabel(title);
        label->setStyleSheet("font-weight: 600; margin-top: 4px;");
        return label;
    };

    root->addWidget(sectionHeader(tr("Structured headers")));
    m_headers = new QPlainTextEdit(this);
    m_headers->setReadOnly(true);
    m_headers->setMinimumHeight(120);
    m_headers->setPlaceholderText(tr("Headers not available"));
    root->addWidget(m_headers, 1);

    root->addWidget(sectionHeader(tr("Body / SDP")));
    m_body = new QPlainTextEdit(this);
    m_body->setReadOnly(true);
    m_body->setMinimumHeight(100);
    m_body->setPlaceholderText(tr("Body not available"));
    root->addWidget(m_body, 1);

    root->addWidget(sectionHeader(tr("Raw SIP")));
    m_raw = new QPlainTextEdit(this);
    m_raw->setReadOnly(true);
    m_raw->setMinimumHeight(160);
    m_raw->setPlaceholderText(tr("Raw SIP not available"));
    root->addWidget(m_raw, 2);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    m_copyRaw = new QPushButton(tr("Copy raw SIP"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    buttons->addWidget(m_copyRaw);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(m_copyRaw, &QPushButton::clicked, this, &SipMessageDetailsDialog::onCopyRawClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void SipMessageDetailsDialog::setTrace(const SipMessageTrace &trace)
{
    m_trace = trace;

    const QString summary = trace.summary();
    m_title->setText(summary.isEmpty() ? tr("SIP Message") : summary);
    m_summary->setText(tr("A detailed view of the selected SIP message. Hover data stays compact; this dialog shows the full context."));
    m_timestamp->setText(trace.timestamp.isValid()
        ? trace.timestamp.toString(Qt::ISODateWithMs)
        : tr("Unknown"));
    m_direction->setText(formatDirection(trace.direction));
    m_method->setText(trace.statusCode > 0
        ? QStringLiteral("%1 %2").arg(trace.statusCode).arg(trace.statusText)
        : trace.method);
    m_from->setText(trace.fromUri.isEmpty() ? tr("Not available") : trace.fromUri);
    m_to->setText(trace.toUri.isEmpty() ? tr("Not available") : trace.toUri);
    m_callId->setText(trace.callId.isEmpty() ? tr("Not available") : trace.callId);
    m_cseq->setText(trace.cSeq.isEmpty() ? tr("Not available") : trace.cSeq);
    m_headers->setPlainText(formatHeaders(trace));

    const QString body = extractBody(trace.rawSip);
    if (body.isEmpty()) {
        m_body->setPlainText(trace.rawSip.isEmpty()
            ? tr("Raw SIP not available")
            : tr("No body present"));
    } else {
        m_body->setPlainText(body);
    }

    if (trace.rawSip.isEmpty()) {
        m_raw->setPlainText(tr("Raw SIP not available"));
        m_copyRaw->setEnabled(false);
    } else {
        m_raw->setPlainText(trace.rawSip);
        m_copyRaw->setEnabled(true);
    }
}

void SipMessageDetailsDialog::onCopyRawClicked()
{
    if (m_trace.rawSip.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_trace.rawSip);
}

QString SipMessageDetailsDialog::formatDirection(SipMessageTrace::Direction direction)
{
    return direction == SipMessageTrace::Direction::Outbound
        ? QObject::tr("Outbound")
        : QObject::tr("Inbound");
}

QString SipMessageDetailsDialog::formatHeaders(const SipMessageTrace &trace)
{
    QString out;
    QTextStream ts(&out);
    ts << "Direction: " << formatDirection(trace.direction) << '\n';
    ts << "Method / Status: "
       << (trace.statusCode > 0
               ? QStringLiteral("%1 %2").arg(trace.statusCode).arg(trace.statusText)
               : trace.method)
       << '\n';
    ts << "Timestamp: " << (trace.timestamp.isValid()
                              ? trace.timestamp.toString(Qt::ISODateWithMs)
                              : QStringLiteral("Unknown")) << '\n';
    ts << "From: " << (trace.fromUri.isEmpty() ? QStringLiteral("Not available") : trace.fromUri) << '\n';
    ts << "To: " << (trace.toUri.isEmpty() ? QStringLiteral("Not available") : trace.toUri) << '\n';
    ts << "Call-ID: " << (trace.callId.isEmpty() ? QStringLiteral("Not available") : trace.callId) << '\n';
    ts << "CSeq: " << (trace.cSeq.isEmpty() ? QStringLiteral("Not available") : trace.cSeq) << '\n';
    return out;
}

QString SipMessageDetailsDialog::extractBody(const QString &rawSip)
{
    const QString normalized = normalizeLineEndings(rawSip);
    const int sep = normalized.indexOf("\n\n");
    if (sep < 0)
        return QString();
    return normalized.mid(sep + 2).trimmed();
}
