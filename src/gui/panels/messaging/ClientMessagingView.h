#pragma once
#include <QByteArray>
#include <QWidget>

#include "sip/MessageHistoryEntry.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

class ClientMessagingController;
class FileTransferModel;

// Client Messaging View (Task W111): the end-user-facing conversational
// surface, embedded inside the Clients page. Presentation only — every
// action here is delegated to ClientMessagingController, which in turn
// reuses the existing MessageHistoryStore/MessagingTransportPolicy/
// PresenceStore/TypingIndicatorController/SipManager exactly as the
// technical Messaging Diagnostics / Presence / MSRP pages already do. No
// protocol logic is duplicated in this widget.
class ClientMessagingView : public QWidget
{
    Q_OBJECT
public:
    explicit ClientMessagingView(QWidget *parent = nullptr);

    // Called by ConversationWorkspacePanel (Task W112) when the selected
    // conversation / active call peer changes — that panel is now the sole
    // conversation picker; this view no longer has its own inline selector.
    void setPeerUri(const QString &peerUri);
    QString peerUri() const { return m_peerUri; }

    // Task W112: exposes the controller (and therefore the single shared
    // ConversationModel instance) so ConversationWorkspacePanel can read the
    // same conversation state instead of owning a second, duplicate model.
    ClientMessagingController *controller() const { return m_controller; }

private slots:
    void onSendClicked();
    void onSendFileClicked();
    void onSaveReceivedFileClicked();
    void onBodyTextChanged();
    void onConversationUpdated(const QString &peerUri);
    void onPresenceUpdated();
    void onMsrpFileTransferReceived(const QString &contentType, const QString &suggestedFileName,
                                    const QByteArray &body, const QString &msrpMessageId);
    void refreshTransportStatus();
    void refreshCapabilities();
    void onAdvancedOptionsToggled(bool expanded);

private:
    void appendHistoryRow(const MessageHistoryEntry &entry);
    void reloadHistory();
    QString currentPeer() const;

    ClientMessagingController *m_controller{nullptr};
    FileTransferModel *m_fileTransferModel{nullptr};

    QLineEdit      *m_toUriEdit{nullptr};
    QLabel         *m_presenceIndicator{nullptr};
    QLabel         *m_typingIndicator{nullptr};
    // Task W113F: transport/content-type/delivery-receipt controls moved
    // behind this collapsed-by-default disclosure ("Messaging options") so
    // the default Client view only shows contact/history/composer/Send —
    // see docs/client-messaging-simplified.md.
    QToolButton    *m_advancedToggle{nullptr};
    QWidget        *m_advancedHost{nullptr};
    QComboBox      *m_transportSelector{nullptr};
    QComboBox      *m_contentTypeSelector{nullptr};
    QCheckBox      *m_requestDeliveredCheck{nullptr};
    QCheckBox      *m_requestDisplayedCheck{nullptr};
    QListWidget    *m_historyList{nullptr};
    QPlainTextEdit *m_inputEdit{nullptr};
    QPushButton    *m_sendBtn{nullptr};
    QPushButton    *m_sendFileBtn{nullptr};
    QPushButton    *m_saveFileBtn{nullptr};
    QLabel         *m_actualTransportLabel{nullptr};
    QLabel         *m_fallbackStatusLabel{nullptr};
    QLabel         *m_sessionStatusLabel{nullptr};

    QString m_peerUri; // externally-driven target (dial target / active call peer)

    // Most recently received-but-not-yet-saved MSRP file transfer offer
    // (Faza 10: no automatic disk write, no local path exposure in history —
    // the user must explicitly choose where to save via
    // onSaveReceivedFileClicked). m_pendingFilePeer scopes the Save button
    // to the conversation it actually belongs to (Faza 14 isolation) —
    // switching to a different conversation hides it rather than offering to
    // save someone else's file under the wrong context.
    QString    m_pendingFilePeer;
    QString    m_pendingFileName;
    QString    m_pendingFileContentType;
    QByteArray m_pendingFileBody;
    QString    m_pendingFileMessageId;
    void updateSaveButtonVisibility();
};
