#include "ConversationFilterProxyModel.h"
#include "ConversationListModel.h"

ConversationFilterProxyModel::ConversationFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    // Ordering is fully encoded in lessThan() below (pinned-first, then
    // most-recent-activity-first) — Qt::AscendingOrder here just means
    // "don't invert what lessThan already decided".
    sort(0, Qt::AscendingOrder);
}

void ConversationFilterProxyModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    invalidateFilter();
}

bool ConversationFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_searchText.trimmed().isEmpty())
        return true;

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString name = index.data(ConversationListModel::DisplayNameRole).toString();
    const QString peer = index.data(ConversationListModel::PeerUriRole).toString();
    const QString preview = index.data(ConversationListModel::LastMessagePreviewRole).toString();

    return name.contains(m_searchText, Qt::CaseInsensitive)
        || peer.contains(m_searchText, Qt::CaseInsensitive)
        || preview.contains(m_searchText, Qt::CaseInsensitive);
}

bool ConversationFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const bool leftPinned = left.data(ConversationListModel::PinnedRole).toBool();
    const bool rightPinned = right.data(ConversationListModel::PinnedRole).toBool();
    if (leftPinned != rightPinned)
        return leftPinned; // pinned rows sort first

    const QDateTime leftActivity = left.data(ConversationListModel::LastActivityRole).toDateTime();
    const QDateTime rightActivity = right.data(ConversationListModel::LastActivityRole).toDateTime();
    return leftActivity > rightActivity; // most-recent-activity rows sort first
}
