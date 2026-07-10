#pragma once

#include <QWidget>

#include "sip/MessageHistoryEntry.h"
#include "sip/MessagingEvent.h"
#include "sip/TypingIndicatorController.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimer;
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
    ~MessagingDiagnosticsPage() override;

private slots:
    void onEventAppended(const MessagingEvent &event);
    void onCleared();
    void applyFilters();
    void onClear();
    void onExportText();
    void onExportJson();
    void onExportInteropJson();
    void onRowActivated(int row, int column);

    void onSendClicked();
    void updateSendEnabled();
    void onEnableSipMessageToggled(bool on);
    void onEnableCpimToggled(bool on);
    void onRequestImdnToggled(bool on);
    void onAutoSendDeliveredToggled(bool on);
    void onAutoSendDisplayedToggled(bool on);
    void onEnableIsComposingToggled(bool on);
    void onAutoTypingNotificationsToggled(bool on);
    void onBodyTextChangedForTyping();
    void onSendIsComposingRequested(IsComposingInfo::State state, int refreshSeconds);
    void onTypingIndicatorExpired();

    void onHistoryEntryAppended(const MessageHistoryEntry &entry);
    void onHistoryEntryUpdated(const MessageHistoryEntry &entry);
    void onHistoryCleared();
    void applyHistoryFilters();
    void onClearHistory();
    void onMarkAsReadClicked();
    void onHistorySelectionChanged();

private:
    void addRow(const MessagingEvent &event);
    void rebuildTable();
    bool passesFilters(const MessagingEvent &event) const;

    void addHistoryRow(const MessageHistoryEntry &entry);
    void rebuildHistoryTable();
    bool passesHistoryFilters(const MessageHistoryEntry &entry) const;

    QTableWidget *m_table{nullptr};
    QLineEdit    *m_callIdFilter{nullptr};
    QComboBox    *m_kindFilter{nullptr};
    QComboBox    *m_directionFilter{nullptr};
    QPushButton  *m_clearBtn{nullptr};
    QPushButton  *m_exportTextBtn{nullptr};
    QPushButton  *m_exportJsonBtn{nullptr};
    QPushButton  *m_exportInteropJsonBtn{nullptr}; // Task W094

    // SIP MESSAGE composer (Task W092)
    QLineEdit      *m_toUriEdit{nullptr};
    QComboBox      *m_composeContentType{nullptr};
    QPlainTextEdit *m_bodyEdit{nullptr};
    QCheckBox      *m_enableSipMessageCheck{nullptr};
    QCheckBox      *m_enableCpimCheck{nullptr};
    QCheckBox      *m_requestImdnCheck{nullptr};
    // IMDN Foundation (Task W096) — auto-notification policy toggles.
    QCheckBox      *m_autoSendDeliveredCheck{nullptr};
    QCheckBox      *m_autoSendDisplayedCheck{nullptr};
    QPushButton    *m_sendBtn{nullptr};
    QLabel         *m_sendStatusLabel{nullptr};
    // Active is-composing (Task W097).
    QCheckBox      *m_enableIsComposingCheck{nullptr};
    QCheckBox      *m_autoTypingNotificationsCheck{nullptr};
    QLabel         *m_typingIndicatorLabel{nullptr};
    TypingIndicatorController *m_typingController{nullptr};
    QTimer         *m_typingIndicatorExpiryTimer{nullptr};
    QString         m_typingIndicatorPeer; // normalized peer the indicator currently reflects

    // Message History (Task W093) — separate table/store from the
    // diagnostics feed above; see MessageHistoryStore for why.
    QTableWidget *m_historyTable{nullptr};
    QComboBox    *m_historyDirectionFilter{nullptr};
    QComboBox    *m_historyContentTypeFilter{nullptr};
    QPushButton  *m_clearHistoryBtn{nullptr};
    // Task W096: manual "mark as read" action for the currently selected
    // inbound row — sends a Displayed IMDN when the sender requested one
    // and AppSettings::autoSendDisplayedImdn is off.
    QPushButton  *m_markAsReadBtn{nullptr};

    QList<MessagingEvent>      m_events;
    QList<MessageHistoryEntry> m_historyEntries;
};
