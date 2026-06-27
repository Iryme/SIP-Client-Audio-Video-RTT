#pragma once

#include <QList>
#include <QWidget>

#include "core/Logger.h"

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

class DiagnosticsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsPanel(QWidget *parent = nullptr);

    QTableWidget *logTable() const;

private slots:
    void onEntryAdded(const LogEntry &entry);
    void onClear();
    void onCopySelected();
    void onExportVisible();
    void onExportBundle();
    void onFilterChanged();
    void onAutoScrollChanged(bool checked);

private:
    enum class SeverityFilter {
        All,
        Debug,
        Info,
        Warning,
        Error,
        Raw
    };

    void buildToolbar(QHBoxLayout *row);
    void refreshTable();
    void updateStatus();
    bool isLevelVisible(LogLevel level) const;
    bool entryMatchesFilters(const LogEntry &entry) const;
    QString currentFilterSummary() const;
    QString componentKeyForCategory(LogCategory category) const;
    QString severityKeyForLevel(LogLevel level) const;
    QString formatLogLine(const LogEntry &entry) const;
    QString visibleLogsAsText() const;

    QStackedWidget *m_viewStack{nullptr};
    QTableWidget   *m_table{nullptr};
    QLabel         *m_emptyLabel{nullptr};
    QLabel         *m_statusLabel{nullptr};
    QLineEdit      *m_search{nullptr};
    QComboBox      *m_severityFilter{nullptr};
    QComboBox      *m_componentFilter{nullptr};
    QCheckBox      *m_autoScroll{nullptr};
    QList<LogEntry> m_entries;
};
