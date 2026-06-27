#pragma once

#include <QWidget>
#include <QList>

#include "core/Logger.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTableWidget;
class QHBoxLayout;
class QTabWidget;

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
    void onLevelToggled(bool checked);
    void onFilterChanged();

private:
    void buildToolbar(QHBoxLayout *row);
    void refreshTable();
    bool isLevelVisible(LogLevel level) const;
    bool entryMatchesFilters(const LogEntry &entry) const;

    QTableWidget *m_table{nullptr};
    QLineEdit    *m_search{nullptr};
    QComboBox    *m_componentFilter{nullptr};
    QList<LogEntry> m_entries;

    QToolButton *m_btnInfo{nullptr};
    QToolButton *m_btnWarn{nullptr};
    QToolButton *m_btnError{nullptr};
    QToolButton *m_btnDebug{nullptr};
    QToolButton *m_btnRaw{nullptr};
};
