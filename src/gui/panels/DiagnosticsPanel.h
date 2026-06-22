#pragma once
#include <QWidget>
#include "diagnostics/LogEntry.h"

class LogFilterModel;
class QToolButton;
class QTableWidget;
class QLineEdit;
class QPushButton;
class QComboBox;
class QHBoxLayout;

class DiagnosticsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsPanel(QWidget *parent = nullptr);

private slots:
    void onEntryAdded(const LogEntry &entry);
    void onFilterChanged();
    void onClear();
    void onCopySelected();
    void onExportVisible();
    void onExportBundle();
    void onRawToggled(bool checked);

private:
    void buildToolbar(QHBoxLayout *row);
    void rebuildTable();
    void appendRowToTable(const LogEntry &entry);
    void saveToggleStates();
    void loadToggleStates();

    LogFilterModel *m_filterModel{nullptr};
    QTableWidget   *m_table{nullptr};
    QLineEdit      *m_search{nullptr};
    QComboBox      *m_categoryCombo{nullptr};

    QToolButton    *m_btnInfo{nullptr};
    QToolButton    *m_btnWarn{nullptr};
    QToolButton    *m_btnError{nullptr};
    QToolButton    *m_btnDebug{nullptr};
    QToolButton    *m_btnRaw{nullptr};
};
