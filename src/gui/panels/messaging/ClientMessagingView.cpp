#include "ClientMessagingView.h"
#include "ClientMessagingController.h"
#include "ConversationModel.h"

#include "core/AppSettings.h"
#include "msrp/MsrpSessionStore.h"
#include "sip/PresenceInfo.h"
#include "sip/PresenceStore.h"
#include "sip/SipManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString formatHistoryRow(const MessageHistoryEntry &e)
{
    const QString who = e.direction == MessageHistoryEntry::Direction::Outbound
        ? QObject::tr("You") : e.peerUri;
    QString status;
    if (e.direction == MessageHistoryEntry::Direction::Outbound) {
        status = MessageHistoryEntry::outboundStatusToString(e.outboundStatus);
        if (e.deliveryState != MessageHistoryEntry::DeliveryState::None)
            status += QStringLiteral(" / ") + MessageHistoryEntry::deliveryStateToString(e.deliveryState);
        if (!e.actualTransport.isEmpty())
            status += QStringLiteral(" [") + e.actualTransport + QLatin1Char(']');
    } else if (e.isTypingNotification) {
        status = QStringLiteral("typing: %1").arg(e.typingState);
    } else if (e.isImdnReport) {
        status = QStringLiteral("IMDN report");
    }
    const QString time = e.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    return QStringLiteral("[%1] %2: %3%4").arg(time, who, e.bodyPreview,
        status.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(status));
}

} // namespace

ClientMessagingView::ClientMessagingView(QWidget *parent)
    : QWidget(parent)
    , m_controller(new ClientMessagingController(this))
{
    setObjectName(QStringLiteral("ClientMessagingView"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ---- Conversation / target selection ----
    auto *targetRow = new QHBoxLayout();
    m_conversationSelector = new QComboBox(this);
    m_conversationSelector->setObjectName(QStringLiteral("messagingContactSelector"));
    m_conversationSelector->setEditable(false);
    m_conversationSelector->setMinimumWidth(160);
    targetRow->addWidget(new QLabel(tr("Conversation:"), this));
    targetRow->addWidget(m_conversationSelector, 1);

    m_toUriEdit = new QLineEdit(this);
    m_toUriEdit->setObjectName(QStringLiteral("messagingToUriEdit"));
    m_toUriEdit->setPlaceholderText(tr("SIP URI to message"));
    targetRow->addWidget(m_toUriEdit, 1);
    root->addLayout(targetRow);

    // ---- Presence / typing status ----
    auto *statusRow = new QHBoxLayout();
    m_presenceIndicator = new QLabel(tr("Presence: Unknown"), this);
    m_presenceIndicator->setObjectName(QStringLiteral("messagingPresenceIndicator"));
    m_typingIndicator = new QLabel(this);
    m_typingIndicator->setObjectName(QStringLiteral("messagingTypingIndicator"));
    statusRow->addWidget(m_presenceIndicator);
    statusRow->addWidget(m_typingIndicator, 1);
    root->addLayout(statusRow);

    // ---- Transport / content-type / IMDN selectors ----
    auto *optionsGroup = new QGroupBox(tr("Send options"), this);
    auto *optionsLayout = new QHBoxLayout(optionsGroup);
    m_transportSelector = new QComboBox(optionsGroup);
    m_transportSelector->setObjectName(QStringLiteral("messagingTransportSelector"));
    m_transportSelector->addItem(tr("Automatic"), QStringLiteral("automatic"));
    m_transportSelector->addItem(tr("SIP MESSAGE"), QStringLiteral("sip-message-only"));
    m_transportSelector->addItem(tr("MSRP Preferred"), QStringLiteral("msrp-preferred"));
    m_transportSelector->addItem(tr("MSRP Required"), QStringLiteral("msrp-required"));
    m_transportSelector->setCurrentIndex(
        m_transportSelector->findData(AppSettings::messagingTransportMode()));
    connect(m_transportSelector, &QComboBox::currentIndexChanged, this, [this](int) {
        AppSettings::setMessagingTransportMode(m_transportSelector->currentData().toString());
    });
    optionsLayout->addWidget(new QLabel(tr("Transport:"), optionsGroup));
    optionsLayout->addWidget(m_transportSelector);

    m_contentTypeSelector = new QComboBox(optionsGroup);
    m_contentTypeSelector->setObjectName(QStringLiteral("messagingContentTypeSelector"));
    m_contentTypeSelector->addItem(tr("Plain text"), static_cast<int>(MessagingContentKind::PlainText));
    m_contentTypeSelector->addItem(tr("HTML"), static_cast<int>(MessagingContentKind::Html));
    m_contentTypeSelector->addItem(tr("CPIM"), static_cast<int>(MessagingContentKind::Cpim));
    optionsLayout->addWidget(new QLabel(tr("Content:"), optionsGroup));
    optionsLayout->addWidget(m_contentTypeSelector);

    m_requestDeliveredCheck = new QCheckBox(tr("Delivered"), optionsGroup);
    m_requestDeliveredCheck->setObjectName(QStringLiteral("messagingRequestDelivered"));
    m_requestDeliveredCheck->setChecked(AppSettings::requestImdnByDefault());
    m_requestDisplayedCheck = new QCheckBox(tr("Displayed"), optionsGroup);
    m_requestDisplayedCheck->setObjectName(QStringLiteral("messagingRequestDisplayed"));
    m_requestDisplayedCheck->setChecked(AppSettings::requestImdnByDefault());
    optionsLayout->addWidget(new QLabel(tr("Request IMDN:"), optionsGroup));
    optionsLayout->addWidget(m_requestDeliveredCheck);
    optionsLayout->addWidget(m_requestDisplayedCheck);
    optionsLayout->addStretch(1);
    root->addWidget(optionsGroup);

    // ---- History ----
    m_historyList = new QListWidget(this);
    m_historyList->setObjectName(QStringLiteral("messagingHistory"));
    root->addWidget(m_historyList, 1);

    // ---- Transport outcome / session status ----
    auto *statusOutcomeRow = new QHBoxLayout();
    m_actualTransportLabel = new QLabel(this);
    m_actualTransportLabel->setObjectName(QStringLiteral("messagingActualTransport"));
    m_fallbackStatusLabel = new QLabel(this);
    m_fallbackStatusLabel->setObjectName(QStringLiteral("messagingFallbackStatus"));
    m_sessionStatusLabel = new QLabel(this);
    m_sessionStatusLabel->setObjectName(QStringLiteral("messagingSessionStatus"));
    statusOutcomeRow->addWidget(m_actualTransportLabel);
    statusOutcomeRow->addWidget(m_fallbackStatusLabel);
    statusOutcomeRow->addWidget(m_sessionStatusLabel, 1);
    root->addLayout(statusOutcomeRow);

    // ---- Composer ----
    m_inputEdit = new QPlainTextEdit(this);
    m_inputEdit->setObjectName(QStringLiteral("messagingInput"));
    m_inputEdit->setPlaceholderText(tr("Type a message…"));
    m_inputEdit->setMaximumHeight(80);
    root->addWidget(m_inputEdit);

    auto *sendRow = new QHBoxLayout();
    m_sendFileBtn = new QPushButton(tr("Send File (Experimental)"), this);
    m_sendFileBtn->setObjectName(QStringLiteral("messagingSendFile"));
    m_sendBtn = new QPushButton(tr("Send"), this);
    m_sendBtn->setObjectName(QStringLiteral("messagingSend"));
    sendRow->addWidget(m_sendFileBtn);
    sendRow->addStretch(1);
    sendRow->addWidget(m_sendBtn);
    root->addLayout(sendRow);

    connect(m_conversationSelector, &QComboBox::currentIndexChanged,
            this, &ClientMessagingView::onConversationSelectionChanged);
    connect(m_sendBtn, &QPushButton::clicked, this, &ClientMessagingView::onSendClicked);
    connect(m_sendFileBtn, &QPushButton::clicked, this, &ClientMessagingView::onSendFileClicked);
    connect(m_inputEdit, &QPlainTextEdit::textChanged, this, &ClientMessagingView::onBodyTextChanged);
    connect(m_controller->conversationModel(), &ConversationModel::conversationUpdated,
            this, &ClientMessagingView::onConversationUpdated);
    connect(m_controller->conversationModel(), &ConversationModel::conversationListChanged,
            this, &ClientMessagingView::onConversationListChanged);
    connect(&PresenceStore::instance(), &PresenceStore::presenceUpdated,
            this, &ClientMessagingView::onPresenceUpdated);
    connect(&MsrpSessionStore::instance(), &MsrpSessionStore::sessionUpdated,
            this, &ClientMessagingView::refreshCapabilities);

    reloadConversationList();
    refreshCapabilities();
}

void ClientMessagingView::setPeerUri(const QString &peerUri)
{
    if (peerUri.trimmed().isEmpty() || m_peerUri == peerUri)
        return;
    m_peerUri = peerUri;
    m_toUriEdit->setText(peerUri);
    reloadHistory();
    refreshCapabilities();
}

QString ClientMessagingView::currentPeer() const
{
    const QString typed = m_toUriEdit->text().trimmed();
    return typed.isEmpty() ? m_peerUri : typed;
}

void ClientMessagingView::onConversationSelectionChanged()
{
    const QString peer = m_conversationSelector->currentData().toString();
    if (!peer.isEmpty())
        m_toUriEdit->setText(peer);
    reloadHistory();
    refreshCapabilities();
}

void ClientMessagingView::onSendClicked()
{
    const QString peer = currentPeer();
    if (peer.trimmed().isEmpty()) {
        m_fallbackStatusLabel->setText(tr("No recipient"));
        return;
    }

    ClientMessagingController::SendOptions opts;
    opts.toUri = peer;
    opts.body = m_inputEdit->toPlainText();
    opts.contentType = static_cast<MessagingContentKind>(m_contentTypeSelector->currentData().toInt());
    opts.requestImdn = m_requestDeliveredCheck->isChecked() || m_requestDisplayedCheck->isChecked();

    QString error;
    if (m_controller->sendMessage(opts, error)) {
        m_inputEdit->clear();
        m_fallbackStatusLabel->clear();
    } else {
        m_fallbackStatusLabel->setText(tr("Send failed: %1").arg(error));
    }
    reloadHistory();
}

void ClientMessagingView::onSendFileClicked()
{
    // Task W111 Phase 6: file transfer wiring lands in a follow-up commit;
    // kept as a visibly disabled/experimental action rather than a dead
    // button that silently does nothing.
    m_fallbackStatusLabel->setText(tr("File transfer is experimental and not yet wired here"));
}

void ClientMessagingView::onBodyTextChanged()
{
    const QString peer = currentPeer();
    if (peer.trimmed().isEmpty())
        return;
    m_controller->notifyComposingTextChanged(peer, !m_inputEdit->toPlainText().trimmed().isEmpty());
}

void ClientMessagingView::onConversationUpdated(const QString &peerUri)
{
    if (ConversationModel::normalizePeer(currentPeer()) == peerUri)
        reloadHistory();
}

void ClientMessagingView::onConversationListChanged()
{
    reloadConversationList();
}

void ClientMessagingView::onPresenceUpdated()
{
    refreshCapabilities();
}

void ClientMessagingView::reloadConversationList()
{
    const QString previouslySelected = m_conversationSelector->currentData().toString();
    m_conversationSelector->blockSignals(true);
    m_conversationSelector->clear();
    for (const QString &peer : m_controller->conversationModel()->conversationPeers())
        m_conversationSelector->addItem(peer, peer);
    const int idx = m_conversationSelector->findData(previouslySelected);
    if (idx >= 0)
        m_conversationSelector->setCurrentIndex(idx);
    m_conversationSelector->blockSignals(false);
}

void ClientMessagingView::reloadHistory()
{
    m_historyList->clear();
    const QString peer = currentPeer();
    if (peer.trimmed().isEmpty())
        return;
    for (const MessageHistoryEntry &e : m_controller->conversationModel()->historyFor(peer))
        appendHistoryRow(e);
    m_historyList->scrollToBottom();
}

void ClientMessagingView::appendHistoryRow(const MessageHistoryEntry &entry)
{
    auto *item = new QListWidgetItem(formatHistoryRow(entry), m_historyList);
    item->setToolTip(tr("Message-ID: %1\nCall-ID: %2\nContent-Type: %3")
        .arg(entry.messageId.isEmpty() ? tr("(none)") : entry.messageId,
             entry.callId.isEmpty() ? tr("(none)") : entry.callId,
             entry.contentType));
}

void ClientMessagingView::refreshTransportStatus()
{
    const auto history = m_controller->conversationModel()->historyFor(currentPeer());
    for (auto it = history.crbegin(); it != history.crend(); ++it) {
        if (it->direction == MessageHistoryEntry::Direction::Outbound && !it->actualTransport.isEmpty()) {
            m_actualTransportLabel->setText(tr("Actual: %1").arg(it->actualTransport));
            m_fallbackStatusLabel->setText(it->fallbackReason.isEmpty() ? QString()
                : tr("Fallback: %1").arg(it->fallbackReason));
            return;
        }
    }
    m_actualTransportLabel->clear();
    m_fallbackStatusLabel->clear();
}

void ClientMessagingView::refreshCapabilities()
{
    const QString peer = currentPeer();

    // Presence
    if (peer.trimmed().isEmpty()) {
        m_presenceIndicator->setText(tr("Presence: Unknown"));
    } else {
        const PresenceInfo info = PresenceStore::instance().current(peer);
        if (info.subscriptionState == PresenceInfo::SubscriptionState::Unknown) {
            m_presenceIndicator->setText(tr("Presence: Not available"));
        } else {
            m_presenceIndicator->setText(tr("Presence: %1 (%2)")
                .arg(PresenceInfo::extendedStatusToString(info.extendedStatus),
                     PresenceInfo::subscriptionStateToString(info.subscriptionState)));
        }
    }

    // Typing
    const QString typing = m_controller->conversationModel()->remoteTypingState(peer);
    m_typingIndicator->setText(typing.isEmpty() ? QString() : tr("%1 is %2…").arg(peer, typing));

    // MSRP session state for the current call, only when it's actually
    // talking to this peer (never inferred from IP/port alone).
    QString sessionText = tr("MSRP session: none");
    if (!peer.trimmed().isEmpty()
        && SipManager::instance().activeCallRemoteUri().compare(peer, Qt::CaseInsensitive) == 0) {
        const QString callSipId = SipManager::instance().activeCallSipId();
        if (!callSipId.isEmpty()) {
            for (const MsrpSessionInfo &session : MsrpSessionStore::instance().snapshot()) {
                if (session.sipHeaderCallId == callSipId) {
                    sessionText = tr("MSRP session: %1").arg(msrpSessionStateToString(session.state));
                    break;
                }
            }
        }
    }
    m_sessionStatusLabel->setText(sessionText);

    refreshTransportStatus();
}
