#include "CallHistoryFilterProxyModel.h"
#include "CallHistoryListModel.h"

#include <QDateTime>

CallHistoryFilterProxyModel::CallHistoryFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void CallHistoryFilterProxyModel::setSearchText(const QString &text)
{
    m_searchText = text;
    invalidateFilter();
}

void CallHistoryFilterProxyModel::setKindFilter(KindFilter filter)
{
    m_kindFilter = filter;
    invalidateFilter();
}

void CallHistoryFilterProxyModel::setDateFilter(DateFilter filter)
{
    m_dateFilter = filter;
    invalidateFilter();
}

bool CallHistoryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const CallHistoryEntry e = idx.data(CallHistoryListModel::EntryRole).value<CallHistoryEntry>();

    switch (m_kindFilter) {
    case KindFilter::All:                                                     break;
    case KindFilter::Incoming:  if (e.direction != CallDirection::Incoming) return false; break;
    case KindFilter::Outgoing:  if (e.direction != CallDirection::Outgoing) return false; break;
    case KindFilter::Missed:    if (e.result != CallResult::Missed)         return false; break;
    case KindFilter::Failed:    if (e.result != CallResult::Failed)         return false; break;
    case KindFilter::WithVideo: if (!e.hadVideo)                            return false; break;
    case KindFilter::WithRtt:   if (!e.hadRtt)                              return false; break;
    }

    if (m_dateFilter != DateFilter::AllTime) {
        if (e.startTime.isNull())
            return false;
        const QDate today = QDateTime::currentDateTimeUtc().date();
        const QDate entryDate = e.startTime.toUTC().date();
        const qint64 daysAgo = entryDate.daysTo(today);
        switch (m_dateFilter) {
        case DateFilter::Today:       if (daysAgo != 0)                return false; break;
        case DateFilter::Last7Days:   if (daysAgo < 0 || daysAgo > 6)  return false; break;
        case DateFilter::Last30Days:  if (daysAgo < 0 || daysAgo > 29) return false; break;
        case DateFilter::AllTime:                                             break;
        }
    }

    if (!m_searchText.isEmpty()) {
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5 %6")
            .arg(e.displayName, e.remoteUri, e.profileName,
                 callResultName(e.result), e.reason,
                 e.lastSipCode > 0 ? QString::number(e.lastSipCode) : QString());
        if (!haystack.contains(m_searchText, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
