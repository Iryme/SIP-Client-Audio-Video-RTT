#pragma once
#include <QSortFilterProxyModel>
#include <QString>

// Combines live search text with a kind filter (direction/result/media) and
// a quick date-range filter. All three combine with AND semantics. Built on
// top of CallHistoryListModel's custom roles so it needs no CallHistoryStore
// or GUI dependency, and can be unit tested directly.
class CallHistoryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum class KindFilter { All, Incoming, Outgoing, Missed, Failed, WithVideo, WithRtt };
    enum class DateFilter { AllTime, Today, Last7Days, Last30Days };

    explicit CallHistoryFilterProxyModel(QObject *parent = nullptr);

    void setSearchText(const QString &text);
    void setKindFilter(KindFilter filter);
    void setDateFilter(DateFilter filter);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString    m_searchText;
    KindFilter m_kindFilter{KindFilter::All};
    DateFilter m_dateFilter{DateFilter::AllTime};
};
