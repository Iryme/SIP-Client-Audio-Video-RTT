#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

// Task W112 (Conversation Workspace). One row per conversation — the union
// of ContactStore's saved contacts and ConversationModel's peers-with-
// history, deduplicated by normalized peer URI (a saved contact with no
// messages yet still gets a row, realizing Contact -> Conversation).
//
// Kept dependency-free like CallHistoryListModel: this model only ever
// holds a snapshot handed to it via setRows() by the owning widget
// (ConversationWorkspacePanel), which derives rows from ContactStore/
// ConversationModel/PresenceStore/SipManager and re-populates on their
// signals. This model itself needs no QWidget/QApplication and no direct
// store dependency, so it can be unit tested standalone.
struct ConversationRow
{
    QString   peerUri;             // normalized key (ConversationModel::normalizePeer)
    QString   displayName;         // Contact name if known, else peerUri
    QString   lastMessagePreview;  // empty if no history yet
    QDateTime lastActivity;        // invalid if no history yet
    int       unreadCount{0};
    QString   presenceText;
    QString   typingText;
    QString   actualTransportText;
    QString   callStateText;       // empty unless this peer is the active call
    bool      pinned{false};
};

class ConversationListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        PeerUriRole = Qt::UserRole + 1,
        DisplayNameRole,
        LastMessagePreviewRole,
        LastActivityRole,
        UnreadCountRole,
        PresenceTextRole,
        TypingTextRole,
        ActualTransportTextRole,
        CallStateTextRole,
        PinnedRole,
    };

    explicit ConversationListModel(QObject *parent = nullptr);

    void setRows(const QList<ConversationRow> &rows);
    ConversationRow rowAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QList<ConversationRow> m_rows;
};
