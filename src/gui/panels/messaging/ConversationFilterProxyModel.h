#pragma once
#include <QSortFilterProxyModel>
#include <QString>

// Task W112 (Conversation Workspace). Text filter over display name/peer
// URI/last-message preview (same search-box idiom as CallHistoryPanel's
// existing search field), sorted pinned-first then by last-activity
// descending. Built entirely on ConversationListModel's roles, so it has
// no store dependency of its own and can be unit tested directly.
class ConversationFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit ConversationFilterProxyModel(QObject *parent = nullptr);

    void setSearchText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_searchText;
};
