#pragma once

#include <QWidget>

#include "sip/MessagingTraceEntry.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class MessagingMessageDetailsDialog;

// "Messaging Diagnostics" nav page: SIP MESSAGE / CPIM / IMDN / is-composing
// / MSRP-SDP diagnostics feed. Strictly read-only — mirrors SipLadderPage's
// structure but never starts a real MSRP session and does not send/receive
// messages itself; it only observes traces already captured by
// SipTraceLogger via MessagingDiagnosticsStore.
class MessagingDiagnosticsPage : public QWidget
{
    Q_OBJECT
public:
    explicit MessagingDiagnosticsPage(QWidget *parent = nullptr);

private slots:
    void onEntryLogged(const MessagingTraceEntry &entry);
    void onCleared();
    void applyFilters();
    void onClear();
    void onExportText();
    void onExportJson();
    void onRowActivated(int row, int column);

private:
    void addRow(const MessagingTraceEntry &entry, int entryIndex);
    void rebuildTable();
    bool passesFilters(const MessagingTraceEntry &entry) const;

    QTableWidget *m_table{nullptr};
    QLineEdit    *m_callIdFilter{nullptr};
    QComboBox    *m_kindFilter{nullptr};
    QComboBox    *m_directionFilter{nullptr};
    QPushButton  *m_clearBtn{nullptr};
    QPushButton  *m_exportTextBtn{nullptr};
    QPushButton  *m_exportJsonBtn{nullptr};

    QList<MessagingTraceEntry> m_entries;
};
