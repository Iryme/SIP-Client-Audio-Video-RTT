#pragma once
#include <QSortFilterProxyModel>
#include <QString>

#include "core/DiagnosticsTimelineEntry.h"

// Combines live search text with a category filter. Built on top of
// DiagnosticsTimelineModel's roles, same pattern as
// CallHistoryFilterProxyModel — no GUI dependency, unit-testable directly.
class DiagnosticsTimelineFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    // "All" plus one entry per taxonomy category, plus the two severity
    // shortcuts requested by the task ("Warnings" / "Errors" match by
    // severity, not by TimelineCategory::Warning/Error alone).
    enum class CategoryFilter {
        All,
        Registration,
        Call,
        Sip,
        Media,
        Rtp,
        Camera,
        Audio,
        Video,
        Rtt,
        Warnings,
        Errors
    };

    explicit DiagnosticsTimelineFilterProxyModel(QObject *parent = nullptr);

    void setSearchText(const QString &text);
    void setCategoryFilter(CategoryFilter filter);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString        m_searchText;
    CategoryFilter m_categoryFilter{CategoryFilter::All};
};
