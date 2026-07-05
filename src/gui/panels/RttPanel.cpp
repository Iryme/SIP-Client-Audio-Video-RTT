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
#include "rtt/RttTextUtils.h"
#include "core/Logger.h"

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
    connect(m_rttInput, &QLineEdit::textChanged, this, &RttPanel::onRttInputChanged);
    connect(m_rttClear, &QPushButton::clicked, m_rttTranscript, &QTextEdit::clear);

    m_liveUpdateTimer.setInterval(150);
    m_liveUpdateTimer.setSingleShot(true);
    connect(&m_liveUpdateTimer, &QTimer::timeout, this, [this]() {
        m_rttRemoteLive->setPlainText(m_remoteBuffer);
    });

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

    // Wire to SipManager for call-level events (state updates, call ended).
    // Remote text is handled exclusively via RttSession::remoteTextReceived to
    // avoid double-display (SipManager::rttTextReceived is a pass-through from
    // the same source).
    connect(&SipManager::instance(), &SipManager::rttMediaConnected, this, [this]() {
        if (m_rttSession)
            onRttStateChanged(m_rttSession->state());
    });
    connect(&SipManager::instance(), &SipManager::rttMediaDisconnected, this, [this]() {
        if (m_rttSession)
            onRttStateChanged(m_rttSession->state());
    });
    connect(&SipManager::instance(), &SipManager::callDisconnected,
            this, [this](const QString &, const QString &, int) {
        resetRttBuffers();
        updateInputState();
    });
}

void RttPanel::setRttSession(RttSession *session)
{
    if (m_rttSession) {
        m_rttSession->disconnect(this);
    }
    m_rttSession = session;
    resetRttBuffers();
    if (m_rttSession) {
        connect(m_rttSession, &RttSession::rttStateChanged,
                this, &RttPanel::onRttStateChanged);
        connect(m_rttSession, &RttSession::remoteTextReceived,
                this, [this](const QString &text) {
            processRemoteText(text);
        });
        onRttStateChanged(m_rttSession->state());
    } else {
        onRttStateChanged(RttState::Disabled);
    }
}

// Called on every QLineEdit textChanged. Computes the T.140 delta between
// the previously sent text and the current field value, and sends it when
// RTT is Active. Suppressed automatically when we programmatically clear
// the field in onRttSend() because m_prevLocalText is reset to "" first.
void RttPanel::onRttInputChanged(const QString &newText)
{
    const QString delta = rttTxDelta(m_prevLocalText, newText);
    m_prevLocalText = newText;

    if (delta.isEmpty())
        return;

    if (!m_rttSession || !m_rttSession->isActive()) {
        Logger::instance().debug(LogCategory::Sip,
            QStringLiteral("RTT TX delta ignored — session not active (delta len=%1)")
                .arg(delta.length()));
        return;
    }

    Logger::instance().debug(LogCategory::Sip,
        QStringLiteral("RTT TX delta: len=%1").arg(delta.length()));
    m_rttSession->sendText(delta);
}

void RttPanel::onRttSend()
{
    const QString text = m_rttInput->text();
    if (text.isEmpty())
        return;

    // Send a T.140 CR to signal end of this paragraph to the remote peer.
    if (m_rttSession && m_rttSession->isActive())
        m_rttSession->sendText(QStringLiteral("\r"));

    // Add to local transcript.
    m_rttTranscript->append(tr("You: %1").arg(text.trimmed()));

    // Reset prevLocalText BEFORE clearing the field so that the textChanged
    // signal fires with newText="" and delta = rttTxDelta("","") = "" → no BS
    // characters are sent to the remote peer.
    m_prevLocalText.clear();
    m_rttInput->clear();

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
        resetRttBuffers();
        break;
    case RttState::Offered:
        stateText = tr("RTT: Offered (awaiting negotiation)");
        break;
    case RttState::Negotiated:
        stateText = tr("RTT: Negotiated");
        break;
    case RttState::Active:
        // Sync m_prevLocalText to the current field content so that no delta
        // is sent for text that may have been typed before RTT became active.
        m_prevLocalText = m_rttInput->text();
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
        active ? tr("Type RTT message (sent character by character)...")
               : tr("RTT not negotiated — input disabled"));
}

// Processes one T.140 received block and updates the live-typing display.
// BS (U+0008) removes the last accumulated character.
// CR (U+000D) or LF (U+000A) flushes the current buffer to the transcript.
// All other codepoints are appended to the live buffer.
void RttPanel::processRemoteText(const QString &incoming)
{
    // Do NOT trim: spaces are payload and a lone CR/LF must flush the
    // current buffer to the transcript (this is how Enter is signalled).
    if (incoming.isEmpty())
        return;

    Logger::instance().debug(LogCategory::Sip,
        QStringLiteral("RTT RX: incoming len=%1").arg(incoming.length()));

    bool prevWasCR = false;
    for (const QChar ch : incoming) {
        const ushort u = ch.unicode();
        if (u == 0x08) {
            if (!m_remoteBuffer.isEmpty())
                m_remoteBuffer.chop(1);
            prevWasCR = false;
        } else if (u == 0x0D) {
            flushRemoteBuffer();
            prevWasCR = true;
        } else if (u == 0x0A) {
            if (!prevWasCR)
                flushRemoteBuffer();
            prevWasCR = false;
        } else {
            m_remoteBuffer.append(ch);
            prevWasCR = false;
        }
    }

    // Batch rapid RTT packets — update live-typing widget at most once per 150 ms.
    if (!m_liveUpdateTimer.isActive())
        m_liveUpdateTimer.start();
}

void RttPanel::flushRemoteBuffer()
{
    if (!m_remoteBuffer.isEmpty()) {
        m_rttTranscript->append(tr("Remote: %1").arg(m_remoteBuffer));
        m_remoteBuffer.clear();
        m_liveUpdateTimer.stop();
        m_rttRemoteLive->clear();
    }
}

void RttPanel::resetRttBuffers()
{
    m_prevLocalText.clear();
    m_remoteBuffer.clear();
    m_liveUpdateTimer.stop();
    if (m_rttRemoteLive)
        m_rttRemoteLive->clear();
}
