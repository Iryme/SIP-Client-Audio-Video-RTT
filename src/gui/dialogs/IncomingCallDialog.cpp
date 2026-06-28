#include "IncomingCallDialog.h"
#include "sip/SipManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

IncomingCallDialog::IncomingCallDialog(QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("IncomingCallDialog"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setModal(false);

    setStyleSheet(QStringLiteral(
        "#IncomingCallDialog {"
        "  background: #1e1e2e;"
        "  border: 1px solid #3a7bd5;"
        "  border-radius: 8px;"
        "}"
        "QLabel#titleLabel {"
        "  color: #ffffff; font-size: 13px; font-weight: bold;"
        "}"
        "QLabel#nameLabel {"
        "  color: #e0e0e0; font-size: 12px;"
        "}"
        "QLabel#uriLabel {"
        "  color: #888888; font-size: 10px;"
        "}"
        "QPushButton#answerBtn {"
        "  background: #2ecc71; color: #ffffff; border: none;"
        "  border-radius: 4px; padding: 6px 18px; font-size: 12px; font-weight: bold;"
        "}"
        "QPushButton#answerBtn:hover { background: #27ae60; }"
        "QPushButton#rejectBtn {"
        "  background: #e74c3c; color: #ffffff; border: none;"
        "  border-radius: 4px; padding: 6px 18px; font-size: 12px; font-weight: bold;"
        "}"
        "QPushButton#rejectBtn:hover { background: #c0392b; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(8);

    auto *titleLabel = new QLabel(tr("Incoming Call"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("nameLabel"));
    m_nameLabel->setWordWrap(true);

    m_uriLabel = new QLabel(this);
    m_uriLabel->setObjectName(QStringLiteral("uriLabel"));
    m_uriLabel->setWordWrap(true);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    m_answerBtn = new QPushButton(tr("Answer"), this);
    m_answerBtn->setObjectName(QStringLiteral("answerBtn"));
    m_rejectBtn = new QPushButton(tr("Reject"), this);
    m_rejectBtn->setObjectName(QStringLiteral("rejectBtn"));
    btnRow->addStretch(1);
    btnRow->addWidget(m_answerBtn);
    btnRow->addWidget(m_rejectBtn);

    root->addWidget(titleLabel);
    root->addWidget(m_nameLabel);
    root->addWidget(m_uriLabel);
    root->addLayout(btnRow);

    connect(m_answerBtn, &QPushButton::clicked, this, [this]() {
        SipManager::instance().answerCall();
        hide();
    });
    connect(m_rejectBtn, &QPushButton::clicked, this, [this]() {
        SipManager::instance().rejectCall();
        hide();
    });

    setFixedWidth(320);
    adjustSize();
}

void IncomingCallDialog::setRemoteUri(const QString &uri)
{
    // Try to extract a display name from "Display Name <sip:...>" format
    const int ltPos = uri.indexOf(QLatin1Char('<'));
    if (ltPos > 0) {
        const QString displayName = uri.left(ltPos).trimmed();
        m_nameLabel->setText(displayName.isEmpty() ? tr("Unknown caller") : displayName);
        m_uriLabel->setText(uri.mid(ltPos).trimmed());
    } else {
        m_nameLabel->setText(tr("Incoming call"));
        m_uriLabel->setText(uri);
    }
    adjustSize();
}

void IncomingCallDialog::onCallStateChanged(CallState state, const QString &, int)
{
    if (state == CallState::IncomingRinging)
        return;
    // Any state other than IncomingRinging means the call was answered, rejected,
    // cancelled by the remote, or failed — hide the popup.
    hide();
}
