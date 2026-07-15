#include "MediaRequestDialog.h"
#include "sip/SipManager.h"
#include "core/Logger.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MediaRequestDialog::MediaRequestDialog(QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("MediaRequestDialog"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setModal(false);

    setStyleSheet(QStringLiteral(
        "#MediaRequestDialog {"
        "  background: #1e1e2e;"
        "  border: 1px solid #3a5bd5;"
        "  border-radius: 8px;"
        "}"
        "QLabel#titleLabel { color: #ffffff; font-size: 13px; font-weight: bold; }"
        "QLabel#nameLabel  { color: #e0e0e0; font-size: 12px; }"
        "QLabel#uriLabel   { color: #888888; font-size: 10px; }"
        "QLabel#typeLabel  { color: #aabbff; font-size: 11px; font-style: italic; }"
        "QPushButton#acceptBtn {"
        "  background: #2ecc71; color: #ffffff; border: none;"
        "  border-radius: 4px; padding: 6px 18px; font-size: 12px; font-weight: bold;"
        "}"
        "QPushButton#acceptBtn:hover { background: #27ae60; }"
        "QPushButton#ignoreBtn {"
        "  background: #555566; color: #cccccc; border: none;"
        "  border-radius: 4px; padding: 6px 18px; font-size: 12px;"
        "}"
        "QPushButton#ignoreBtn:hover { background: #666677; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(6);

    m_titleLabel = new QLabel(tr("Incoming Media Request"), this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("nameLabel"));
    m_nameLabel->setWordWrap(true);

    m_uriLabel = new QLabel(this);
    m_uriLabel->setObjectName(QStringLiteral("uriLabel"));
    m_uriLabel->setWordWrap(true);

    m_typeLabel = new QLabel(this);
    m_typeLabel->setObjectName(QStringLiteral("typeLabel"));

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    m_acceptBtn = new QPushButton(tr("Accept"), this);
    m_acceptBtn->setObjectName(QStringLiteral("acceptBtn"));
    m_ignoreBtn = new QPushButton(tr("Ignore"), this);
    m_ignoreBtn->setObjectName(QStringLiteral("ignoreBtn"));
    btnRow->addStretch(1);
    btnRow->addWidget(m_acceptBtn);
    btnRow->addWidget(m_ignoreBtn);

    root->addWidget(m_titleLabel);
    root->addWidget(m_nameLabel);
    root->addWidget(m_uriLabel);
    root->addWidget(m_typeLabel);
    root->addLayout(btnRow);

    connect(m_acceptBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentType == MediaType::Video) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Accepting pending incoming video request"));
            SipManager::instance().requestCallVideo(true);
        } else {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Accepting text request: protocol=RTT"));
            SipManager::instance().acceptIncomingRtt();
        }
        hide();
    });

    connect(m_ignoreBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentType == MediaType::Video) {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Incoming video request ignored by user"));
        } else {
            Logger::instance().info(LogCategory::Sip,
                QStringLiteral("Incoming RTT request ignored by user"));
            // Task W109A: explicitly clear the pending state via the SIP
            // layer instead of merely hiding the dialog — the offer was
            // already declined (m=text 0) by the auto-response, so without
            // this the app would keep showing "pending" even though it had
            // already told the peer no, and a later "Accept" click would be
            // ambiguous about what it was accepting.
            SipManager::instance().rejectIncomingRtt();
        }
        hide();
    });

    setFixedWidth(340);
    adjustSize();
}

void MediaRequestDialog::showVideoRequest(const QString &remoteUri)
{
    m_currentType = MediaType::Video;
    updateLayout(MediaType::Video, remoteUri);
    m_titleLabel->setText(tr("Incoming Video Request"));
    m_typeLabel->setText(tr("Request type: Video"));
    m_acceptBtn->setText(tr("Accept Video"));
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Incoming video request pending"));
    adjustSize();
    show();
    raise();
    activateWindow();
}

void MediaRequestDialog::showRttRequest(const QString &remoteUri)
{
    m_currentType = MediaType::Rtt;
    updateLayout(MediaType::Rtt, remoteUri);
    m_titleLabel->setText(tr("Incoming Text Request"));
    m_typeLabel->setText(tr("Request type: RTT (Real-Time Text)"));
    m_acceptBtn->setText(tr("Accept RTT"));
    Logger::instance().info(LogCategory::Sip,
        QStringLiteral("Incoming text request pending: protocol=RTT"));
    adjustSize();
    show();
    raise();
    activateWindow();
}

void MediaRequestDialog::updateLayout(MediaType /*type*/, const QString &remoteUri)
{
    const int ltPos = remoteUri.indexOf(QLatin1Char('<'));
    if (ltPos > 0) {
        const QString displayName = remoteUri.left(ltPos).trimmed();
        m_nameLabel->setText(displayName.isEmpty() ? tr("Unknown caller") : displayName);
        m_uriLabel->setText(remoteUri.mid(ltPos).trimmed());
    } else {
        m_nameLabel->setText(tr("Peer"));
        m_uriLabel->setText(remoteUri);
    }
}

void MediaRequestDialog::onCallStateChanged(CallState state, const QString &, int)
{
    // Dismiss on any terminal state
    if (state == CallState::Idle || state == CallState::Failed
        || state == CallState::IncomingRinging) {
        hide();
    }
}

void MediaRequestDialog::onVideoMediaConnected()
{
    if (m_currentType == MediaType::Video)
        hide();
}

void MediaRequestDialog::onRttMediaConnected()
{
    if (m_currentType == MediaType::Rtt)
        hide();
}
