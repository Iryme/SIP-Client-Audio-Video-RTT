#pragma once
#include <QWidget>

class QLineEdit;
class QListView;
class QPushButton;
class QModelIndex;

class ConversationListModel;
class ConversationFilterProxyModel;
class ClientMessagingController;

// Task W112 (Conversation Workspace): the primary navigation surface for
// the Clients page, inverting the previous call-centric model to
// Contact -> Conversation -> Messaging -> Call. Rows are the union of
// ContactStore's saved contacts and ConversationModel's peers-with-history
// (a saved contact with no messages yet still appears, with an empty
// preview) — see ConversationListModel for the exact row derivation.
//
// This widget owns no protocol logic: it only derives display rows from
// the existing ContactStore/ConversationModel/PresenceStore/SipManager and
// emits intent signals (conversationSelected/callRequested) for the host
// page to act on, exactly like ContactsPanel::dialRequested and
// CallHistoryPanel::redialRequested already do.
class ConversationWorkspacePanel : public QWidget
{
    Q_OBJECT
public:
    // sharedConversationModel is owned elsewhere (ClientMessagingController)
    // — this panel only reads it, so the Clients page has exactly one
    // ConversationModel instance, never a second one duplicating state.
    explicit ConversationWorkspacePanel(ClientMessagingController *messagingController,
                                        QWidget *parent = nullptr);

signals:
    // The user picked a conversation to view/message.
    void conversationSelected(const QString &peerUri);
    // The user asked to start a call with the selected conversation's peer
    // (Contact -> Conversation -> Call, never the reverse).
    void callRequested(const QString &peerUri);

private slots:
    void refreshRows();
    void onSelectionActivated(const QModelIndex &proxyIndex);
    void onCallClicked();
    void onPinToggled();
    void onSearchTextChanged(const QString &text);

private:
    QString selectedPeerUri() const;

    ClientMessagingController   *m_messagingController{nullptr};
    ConversationListModel        *m_model{nullptr};
    ConversationFilterProxyModel *m_proxy{nullptr};

    QLineEdit   *m_searchEdit{nullptr};
    QListView   *m_listView{nullptr};
    QPushButton *m_callBtn{nullptr};
    QPushButton *m_pinBtn{nullptr};
};
