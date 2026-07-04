#pragma once
#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

// "Call History" page — lists recorded calls from CallHistoryStore, with
// Clear History (confirmed) and Export JSON actions and a details dialog
// on double-click / selection.
class CallHistoryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CallHistoryPanel(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onClearHistory();
    void onExportJson();
    void onItemActivated(QListWidgetItem *item);

private:
    void showDetails(const QString &entryId);

    QListWidget *m_list{nullptr};
    QLabel      *m_emptyState{nullptr};
    QPushButton *m_clearBtn{nullptr};
    QPushButton *m_exportBtn{nullptr};
};
