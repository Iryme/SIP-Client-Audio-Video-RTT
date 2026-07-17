#include "ConversationWorkspacePanel.h"

#include "core/AppSettings.h"
#include "core/ContactStore.h"
#include "gui/panels/messaging/ClientMessagingController.h"
#include "gui/panels/messaging/ConversationFilterProxyModel.h"
#include "gui/panels/messaging/ConversationListModel.h"
#include "gui/panels/messaging/ConversationModel.h"
#include "sip/CallStateMachine.h"
#include "sip/PresenceInfo.h"
#include "sip/PresenceStore.h"
#include "sip/SipManager.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

ConversationWorkspacePanel::ConversationWorkspacePanel(ClientMessagingController *messagingController,
                                                       QWidget *parent)
    : QWidget(parent)
    , m_messagingController(messagingController)
{
    setObjectName(QStringLiteral("ConversationWorkspacePanel"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("conversationSearchEdit"));
    m_searchEdit->setPlaceholderText(tr("Search conversations..."));
    m_searchEdit->setClearButtonEnabled(true);
    root->addWidget(m_searchEdit);

    m_model = new ConversationListModel(this);
    m_proxy = new ConversationFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_listView = new QListView(this);
    // Task W111's messagingContactSelector automation id moves here — this
    // list is now the real conversation/contact selector; ClientMessagingView
    // no longer has its own inline combo (Task W112).
    m_listView->setObjectName(QStringLiteral("messagingContactSelector"));
    m_listView->setModel(m_proxy);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_listView, 1);

    auto *actionRow = new QHBoxLayout();
    m_pinBtn = new QPushButton(tr("Pin"), this);
    m_pinBtn->setObjectName(QStringLiteral("conversationPinBtn"));
    m_pinBtn->setCheckable(true);
    m_callBtn = new QPushButton(tr("Call"), this);
    m_callBtn->setObjectName(QStringLiteral("conversationCallBtn"));
    actionRow->addWidget(m_pinBtn);
    actionRow->addStretch(1);
    actionRow->addWidget(m_callBtn);
    root->addLayout(actionRow);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ConversationWorkspacePanel::onSearchTextChanged);
    connect(m_listView, &QListView::clicked, this, &ConversationWorkspacePanel::onSelectionActivated);
    connect(m_listView, &QListView::activated, this, &ConversationWorkspacePanel::onSelectionActivated);
    connect(m_callBtn, &QPushButton::clicked, this, &ConversationWorkspacePanel::onCallClicked);
    connect(m_pinBtn, &QPushButton::clicked, this, &ConversationWorkspacePanel::onPinToggled);

    connect(&ContactStore::instance(), &ContactStore::contactsChanged,
            this, &ConversationWorkspacePanel::refreshRows);
    connect(m_messagingController->conversationModel(), &ConversationModel::conversationListChanged,
            this, &ConversationWorkspacePanel::refreshRows);
    connect(m_messagingController->conversationModel(), &ConversationModel::conversationUpdated,
            this, &ConversationWorkspacePanel::refreshRows);
    connect(&PresenceStore::instance(), &PresenceStore::presenceUpdated,
            this, &ConversationWorkspacePanel::refreshRows);
    connect(&SipManager::instance(), &SipManager::callStateChanged,
            this, &ConversationWorkspacePanel::refreshRows);

    refreshRows();
}

QString ConversationWorkspacePanel::selectedPeerUri() const
{
    const QModelIndex idx = m_listView->currentIndex();
    return idx.isValid() ? idx.data(ConversationListModel::PeerUriRole).toString() : QString();
}

void ConversationWorkspacePanel::onSelectionActivated(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid())
        return;
    const QString peer = proxyIndex.data(ConversationListModel::PeerUriRole).toString();
    if (peer.isEmpty())
        return;
    m_pinBtn->setChecked(proxyIndex.data(ConversationListModel::PinnedRole).toBool());
    emit conversationSelected(peer);
}

void ConversationWorkspacePanel::onCallClicked()
{
    const QString peer = selectedPeerUri();
    if (!peer.isEmpty())
        emit callRequested(peer);
}

void ConversationWorkspacePanel::onPinToggled()
{
    const QString peer = selectedPeerUri();
    if (peer.isEmpty()) {
        m_pinBtn->setChecked(false);
        return;
    }
    AppSettings::setConversationPinned(peer, m_pinBtn->isChecked());
    refreshRows();
}

void ConversationWorkspacePanel::onSearchTextChanged(const QString &text)
{
    m_proxy->setSearchText(text);
}

void ConversationWorkspacePanel::refreshRows()
{
    QList<ConversationRow> rows;
    QSet<QString> seen;
    const QStringList pinned = AppSettings::pinnedConversationPeers();

    auto addOrUpdate = [&](const QString &peerUri, const QString &nameOverride) {
        const QString key = ConversationModel::normalizePeer(peerUri);
        if (key.isEmpty() || seen.contains(key))
            return;
        seen.insert(key);

        ConversationRow row;
        row.peerUri = key;
        row.displayName = nameOverride.isEmpty() ? peerUri : nameOverride;

        ConversationModel *conv = m_messagingController->conversationModel();
        const MessageHistoryEntry last = conv->lastMessageFor(key);
        row.lastMessagePreview = last.id != 0 ? last.bodyPreview : QString();
        row.lastActivity = conv->lastActivityFor(key);
        row.unreadCount = conv->unreadCountFor(key);
        row.typingText = conv->remoteTypingState(key);
        if (last.id != 0 && last.direction == MessageHistoryEntry::Direction::Outbound)
            row.actualTransportText = last.actualTransport;

        const PresenceInfo info = PresenceStore::instance().current(peerUri);
        row.presenceText = info.subscriptionState == PresenceInfo::SubscriptionState::Unknown
            ? tr("Not available")
            : PresenceInfo::extendedStatusToString(info.extendedStatus);

        if (SipManager::instance().activeCallRemoteUri().compare(peerUri, Qt::CaseInsensitive) == 0)
            row.callStateText = callStateDisplayText(SipManager::instance().callState());

        row.pinned = pinned.contains(key);
        rows.append(row);
    };

    for (const Contact &c : ContactStore::instance().contacts())
        addOrUpdate(c.uri, c.name);
    for (const QString &peer : m_messagingController->conversationModel()->conversationPeers())
        addOrUpdate(peer, QString());

    m_model->setRows(rows);
}
