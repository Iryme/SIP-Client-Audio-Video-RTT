#pragma once

#include <QWidget>

#include "sip/MessagingEvent.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class MessagingMessageDetailsDialog;

// "Messaging Diagnostics" nav page: SIP MESSAGE / CPIM / IMDN / is-composing
// / MSRP-SDP diagnostics feed. Strictly read-only — mirrors SipLadderPage's
// structure but never starts a real MSRP session and does not send/receive
// messages itself. The feed is sourced from MessagingEventStore (the
// transport-independent messaging model, Task W091), which itself maps
// entries produced by MessagingDiagnosticsStore (Task W090) — this page does
// not re-implement any CPIM/IMDN/is-composing/SDP-MSRP parsing.
class MessagingDiagnosticsPage : public QWidget
{
    Q_OBJECT
public:
    explicit MessagingDiagnosticsPage(QWidget *parent = nullptr);

private slots:
    void onEventAppended(const MessagingEvent &event);
    void onCleared();
    void applyFilters();
    void onClear();
    void onExportText();
    void onExportJson();
    void onRowActivated(int row, int column);

private:
    void addRow(const MessagingEvent &event);
    void rebuildTable();
    bool passesFilters(const MessagingEvent &event) const;

    QTableWidget *m_table{nullptr};
    QLineEdit    *m_callIdFilter{nullptr};
    QComboBox    *m_kindFilter{nullptr};
    QComboBox    *m_directionFilter{nullptr};
    QPushButton  *m_clearBtn{nullptr};
    QPushButton  *m_exportTextBtn{nullptr};
    QPushButton  *m_exportJsonBtn{nullptr};

    QList<MessagingEvent> m_events;
};
