#include "RttPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QListWidget>
#include <QFrame>
#include "sip/SipManager.h"

RttPanel::RttPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("RttPanel");

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("RttTabs");
    outerLayout->addWidget(tabs);

    // -----------------------------------------------------------------------
    // RTT Tab
    // -----------------------------------------------------------------------
    auto *rttTab = new QWidget();
    auto *rttLayout = new QVBoxLayout(rttTab);
    rttLayout->setContentsMargins(8, 8, 8, 8);
    rttLayout->setSpacing(6);

    m_rttState = new QLabel(tr("RTT: Not negotiated"), rttTab);
    m_rttState->setObjectName("RttState");
    m_rttState->setStyleSheet("color: #aaaaaa; font-size: 11px;");
    rttLayout->addWidget(m_rttState);

    // Remote live typing area
    auto *liveLabel = new QLabel(tr("Remote typing:"), rttTab);
    liveLabel->setStyleSheet("color: #888888; font-size: 10px;");
    rttLayout->addWidget(liveLabel);

    m_rttRemoteLive = new QTextEdit(rttTab);
    m_rttRemoteLive->setObjectName("RttRemoteLive");
    m_rttRemoteLive->setReadOnly(true);
    m_rttRemoteLive->setFixedHeight(48);
    m_rttRemoteLive->setPlaceholderText(tr("(remote is not typing)"));
    rttLayout->addWidget(m_rttRemoteLive);

    // Transcript
    auto *transcriptLabel = new QLabel(tr("Transcript:"), rttTab);
    transcriptLabel->setStyleSheet("color: #888888; font-size: 10px;");
    rttLayout->addWidget(transcriptLabel);

    m_rttTranscript = new QTextEdit(rttTab);
    m_rttTranscript->setObjectName("RttTranscript");
    m_rttTranscript->setReadOnly(true);
    rttLayout->addWidget(m_rttTranscript, 1);

    // Input + buttons
    m_rttInput = new QLineEdit(rttTab);
    m_rttInput->setObjectName("RttInput");
    m_rttInput->setPlaceholderText(tr("RTT not negotiated — input disabled"));
    m_rttInput->setEnabled(false);
    rttLayout->addWidget(m_rttInput);

    auto *rttBtnRow = new QHBoxLayout();
    m_rttClear = new QPushButton(tr("Clear"), rttTab);
    m_rttSend  = new QPushButton(tr("Send"), rttTab);
    m_rttSend->setObjectName("SendBtn");
    m_rttSend->setEnabled(false);
    rttBtnRow->addWidget(m_rttClear);
    rttBtnRow->addStretch();
    rttBtnRow->addWidget(m_rttSend);
    rttLayout->addLayout(rttBtnRow);

    connect(m_rttSend,  &QPushButton::clicked, this, &RttPanel::onRttSend);
    connect(m_rttInput, &QLineEdit::returnPressed, this, &RttPanel::onRttSend);
    connect(m_rttClear, &QPushButton::clicked, m_rttTranscript, &QTextEdit::clear);

    tabs->addTab(rttTab, tr("RTT"));

    // -----------------------------------------------------------------------
    // LMPE Tab
    // -----------------------------------------------------------------------
    auto *lmpeTab = new QWidget();
    auto *lmpeLayout = new QVBoxLayout(lmpeTab);
    lmpeLayout->setContentsMargins(8, 8, 8, 8);
    lmpeLayout->setSpacing(6);

    m_lmpeState = new QLabel(tr("LMPE: Inactive"), lmpeTab);
    m_lmpeState->setObjectName("LmpeState");
    m_lmpeState->setStyleSheet("color: #aaaaaa; font-size: 11px;");
    lmpeLayout->addWidget(m_lmpeState);

    m_lmpeList = new QListWidget(lmpeTab);
    m_lmpeList->setObjectName("LmpeList");
    lmpeLayout->addWidget(m_lmpeList, 1);

    m_lmpeInput = new QLineEdit(lmpeTab);
    m_lmpeInput->setObjectName("LmpeInput");
    m_lmpeInput->setPlaceholderText(tr("Type LMPE message..."));
    lmpeLayout->addWidget(m_lmpeInput);

    auto *lmpeBtnRow = new QHBoxLayout();
    m_lmpeSend = new QPushButton(tr("Send"), lmpeTab);
    m_lmpeSend->setObjectName("SendBtn");
    lmpeBtnRow->addStretch();
    lmpeBtnRow->addWidget(m_lmpeSend);
    lmpeLayout->addLayout(lmpeBtnRow);

    connect(m_lmpeSend,  &QPushButton::clicked, this, &RttPanel::onLmpeSend);
    connect(m_lmpeInput, &QLineEdit::returnPressed, this, &RttPanel::onLmpeSend);

    tabs->addTab(lmpeTab, tr("LMPE"));

    // Wire to SipManager RTT signals for live state updates.
    connect(&SipManager::instance(), &SipManager::rttMediaConnected, this, [this]() {
        if (m_rttSession)
            onRttStateChanged(m_rttSession->state());
    });
    connect(&SipManager::instance(), &SipManager::rttMediaDisconnected, this, [this]() {
        if (m_rttSession)
            onRttStateChanged(m_rttSession->state());
    });
    connect(&SipManager::instance(), &SipManager::rttTextReceived, this, [this](const QString &text) {
        m_rttRemoteLive->setPlainText(text);
        m_rttTranscript->append(tr("Remote: %1").arg(text));
    });
    connect(&SipManager::instance(), &SipManager::callDisconnected, this, [this](const QString &, const QString &, int) {
        m_rttRemoteLive->clear();
        updateInputState();
    });
}

void RttPanel::setRttSession(RttSession *session)
{
    if (m_rttSession) {
        m_rttSession->disconnect(this);
    }
    m_rttSession = session;
    if (m_rttSession) {
        connect(m_rttSession, &RttSession::rttStateChanged,
                this, &RttPanel::onRttStateChanged);
        connect(m_rttSession, &RttSession::remoteTextReceived,
                this, [this](const QString &text) {
            m_rttRemoteLive->setPlainText(text);
            m_rttTranscript->append(tr("Remote: %1").arg(text));
        });
        onRttStateChanged(m_rttSession->state());
    } else {
        onRttStateChanged(RttState::Disabled);
    }
}

void RttPanel::onRttSend()
{
    const QString text = m_rttInput->text().trimmed();
    if (text.isEmpty())
        return;
    m_rttTranscript->append(tr("You: %1").arg(text));
    m_rttInput->clear();

    if (m_rttSession && m_rttSession->isActive()) {
        m_rttSession->sendText(text);
    } else {
        SipManager::instance().sendRttText(text);
    }
    emit rttMessageSent(text);
}

void RttPanel::onLmpeSend()
{
    const QString text = m_lmpeInput->text().trimmed();
    if (text.isEmpty())
        return;
    m_lmpeList->addItem(tr("You: %1").arg(text));
    m_lmpeInput->clear();
    emit lmpeMessageSent(text);
}

void RttPanel::onRttStateChanged(RttState state)
{
    QString stateText;
    switch (state) {
    case RttState::Disabled:
        stateText = tr("RTT: Not negotiated");
        break;
    case RttState::Offered:
        stateText = tr("RTT: Offered (awaiting negotiation)");
        break;
    case RttState::Negotiated:
        stateText = tr("RTT: Negotiated");
        break;
    case RttState::Active:
        stateText = tr("RTT: Active");
        break;
    case RttState::Failed:
        stateText = tr("RTT: Failed");
        break;
    }
    m_rttState->setText(stateText);
    updateInputState();
}

void RttPanel::updateInputState()
{
    const bool active = m_rttSession && m_rttSession->isActive();
    m_rttInput->setEnabled(active);
    m_rttSend->setEnabled(active);
    m_rttInput->setPlaceholderText(
        active ? tr("Type RTT message...")
               : tr("RTT not negotiated — input disabled"));
}
