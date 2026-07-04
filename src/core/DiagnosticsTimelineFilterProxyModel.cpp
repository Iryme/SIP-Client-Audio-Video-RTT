#include "DiagnosticsTimelineFilterProxyModel.h"
#include "DiagnosticsTimelineModel.h"

DiagnosticsTimelineFilterProxyModel::DiagnosticsTimelineFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void DiagnosticsTimelineFilterProxyModel::setSearchText(const QString &text)
{
    m_searchText = text;
    invalidateFilter();
}

void DiagnosticsTimelineFilterProxyModel::setCategoryFilter(CategoryFilter filter)
{
    m_categoryFilter = filter;
    invalidateFilter();
}

bool DiagnosticsTimelineFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const DiagnosticsTimelineEntry e = idx.data(DiagnosticsTimelineModel::EntryRole).value<DiagnosticsTimelineEntry>();

    switch (m_categoryFilter) {
    case CategoryFilter::All:                                                                    break;
    case CategoryFilter::Registration: if (e.category != TimelineCategory::Registration) return false; break;
    case CategoryFilter::Call:         if (e.category != TimelineCategory::Call)         return false; break;
    case CategoryFilter::Sip:          if (e.category != TimelineCategory::Sip)          return false; break;
    case CategoryFilter::Media:        if (e.category != TimelineCategory::Media)        return false; break;
    case CategoryFilter::Rtp:          if (e.category != TimelineCategory::Rtp)          return false; break;
    case CategoryFilter::Camera:       if (e.category != TimelineCategory::Camera)       return false; break;
    case CategoryFilter::Audio:        if (e.category != TimelineCategory::Audio)        return false; break;
    case CategoryFilter::Video:        if (e.category != TimelineCategory::Video)        return false; break;
    case CategoryFilter::Rtt:          if (e.category != TimelineCategory::Rtt)          return false; break;
    case CategoryFilter::Warnings:     if (e.severity != TimelineSeverity::Warning)      return false; break;
    case CategoryFilter::Errors:       if (e.severity != TimelineSeverity::Error)        return false; break;
    }

    if (!m_searchText.isEmpty()) {
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
            .arg(e.title, e.details, e.remoteUri,
                 e.sipCode > 0 ? QString::number(e.sipCode) : QString(),
                 timelineCategoryName(e.category));
        if (!haystack.contains(m_searchText, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
