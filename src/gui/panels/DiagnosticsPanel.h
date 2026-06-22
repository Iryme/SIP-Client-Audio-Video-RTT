#pragma once
#include <QWidget>
#include "core/Logger.h"

class QToolButton;
class QTableWidget;
class QLineEdit;
class QPushButton;
class QHBoxLayout;

class DiagnosticsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsPanel(QWidget *parent = nullptr);

private slots:
    void onEntryAdded(const LogEntry &entry);
    void onClear();
    void onCopySelected();
    void onExportVisible();
    void onExportBundle();
    void onLevelToggled(bool checked);

private:
    void buildToolbar(QHBoxLayout *row);
    void addRow(const LogEntry &entry);
    bool isLevelVisible(LogLevel level) const;

    QTableWidget *m_table{nullptr};
    QLineEdit    *m_search{nullptr};

    QToolButton  *m_btnInfo{nullptr};
    QToolButton  *m_btnWarn{nullptr};
    QToolButton  *m_btnError{nullptr};
    QToolButton  *m_btnDebug{nullptr};
    QToolButton  *m_btnRaw{nullptr};
};
