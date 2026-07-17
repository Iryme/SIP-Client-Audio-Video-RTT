#include "ConversationListModel.h"

ConversationListModel::ConversationListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void ConversationListModel::setRows(const QList<ConversationRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

ConversationRow ConversationListModel::rowAt(int row) const
{
    return (row >= 0 && row < m_rows.size()) ? m_rows.at(row) : ConversationRow();
}

int ConversationListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant ConversationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();

    const ConversationRow &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return row.displayName;
    case PeerUriRole:
        return row.peerUri;
    case LastMessagePreviewRole:
        return row.lastMessagePreview;
    case LastActivityRole:
        return row.lastActivity;
    case UnreadCountRole:
        return row.unreadCount;
    case PresenceTextRole:
        return row.presenceText;
    case TypingTextRole:
        return row.typingText;
    case ActualTransportTextRole:
        return row.actualTransportText;
    case CallStateTextRole:
        return row.callStateText;
    case PinnedRole:
        return row.pinned;
    default:
        return QVariant();
    }
}
