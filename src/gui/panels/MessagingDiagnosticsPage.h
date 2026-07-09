#pragma once

#include <QWidget>

#include "sip/MessagingEvent.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class MessagingMessageDetailsDialog;

// "Messaging Diagnostics" nav page: SIP MESSAGE / CPIM / IMDN / is-composing
// / MSRP-SDP diagnostics feed, plus (Task W092) a simple SIP MESSAGE composer
// and send action. The diagnostics feed itself remains strictly read-only —
// mirrors SipLadderPage's structure and never starts a real MSRP session.
// The feed is sourced from MessagingEventStore (the transport-independent
// messaging model, Task W091), which itself maps entries produced by
// MessagingDiagnosticsStore (Task W090) — this page does not re-implement
// any CPIM/IMDN/is-composing/SDP-MSRP parsing. Composing/sending (Task W092)
// is a separate, explicit, opt-in action gated by AppSettings::enableSipMessage;
// it never touches MSRP.
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

    void onSendClicked();
    void updateSendEnabled();
    void onEnableSipMessageToggled(bool on);
    void onEnableCpimToggled(bool on);
    void onRequestImdnToggled(bool on);

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

    // SIP MESSAGE composer (Task W092)
    QLineEdit      *m_toUriEdit{nullptr};
    QComboBox      *m_composeContentType{nullptr};
    QPlainTextEdit *m_bodyEdit{nullptr};
    QCheckBox      *m_enableSipMessageCheck{nullptr};
    QCheckBox      *m_enableCpimCheck{nullptr};
    QCheckBox      *m_requestImdnCheck{nullptr};
    QPushButton    *m_sendBtn{nullptr};
    QLabel         *m_sendStatusLabel{nullptr};

    QList<MessagingEvent> m_events;
};
