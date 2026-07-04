#pragma once
#include <QWidget>

#include "core/CallHistoryEntry.h"

class QLabel;
class QListView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QModelIndex;
class CallHistoryListModel;
class CallHistoryFilterProxyModel;

// "Call History" page — searchable/filterable list of recorded calls from
// CallHistoryStore, with Clear History (confirmed), Export JSON/CSV, Call
// Back (redial), and a details dialog on activation.
class CallHistoryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CallHistoryPanel(QWidget *parent = nullptr);

signals:
    // Emitted with the entry's remoteUri when the user asks to call back.
    // The panel never talks to SipManager directly — the host window (which
    // already owns the dial UI/state) is responsible for placing the call.
    void redialRequested(const QString &remoteUri);

private slots:
    void refresh();
    void onClearHistory();
    void onExportJson();
    void onExportCsv();
    void onSearchTextChanged(const QString &text);
    void onKindFilterChanged(int index);
    void onDateFilterChanged(int index);
    void onItemActivated(const QModelIndex &index);
    void onRedialSelected();
    void updateResultsLabel();

private:
    void showDetails(const CallHistoryEntry &entry);
    CallHistoryEntry currentSelection() const;

    CallHistoryListModel        *m_model{nullptr};
    CallHistoryFilterProxyModel *m_proxy{nullptr};

    QLineEdit   *m_searchEdit{nullptr};
    QComboBox   *m_kindFilterCombo{nullptr};
    QComboBox   *m_dateFilterCombo{nullptr};
    QLabel      *m_resultsLabel{nullptr};
    QLabel      *m_emptyState{nullptr};
    QListView   *m_list{nullptr};
    QPushButton *m_redialBtn{nullptr};
    QPushButton *m_clearBtn{nullptr};
    QPushButton *m_exportJsonBtn{nullptr};
    QPushButton *m_exportCsvBtn{nullptr};
};
