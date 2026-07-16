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

    // Called by the Clients page when the dial target / active call peer
    // changes, so the messaging surface follows the same "who am I talking
    // to" concept as the call controls without a second target field.
    void setPeerUri(const QString &peerUri);
    QString peerUri() const { return m_peerUri; }

private slots:
    void onConversationSelectionChanged();
    void onSendClicked();
    void onSendFileClicked();
    void onSaveReceivedFileClicked();
    void onBodyTextChanged();
    void onConversationUpdated(const QString &peerUri);
    void onConversationListChanged();
    void onPresenceUpdated();
    void onMsrpFileTransferReceived(const QString &contentType, const QString &suggestedFileName,
                                    const QByteArray &body, const QString &msrpMessageId);
    void refreshTransportStatus();
    void refreshCapabilities();

private:
    void appendHistoryRow(const MessageHistoryEntry &entry);
    void reloadHistory();
    void reloadConversationList();
    QString currentPeer() const;

    ClientMessagingController *m_controller{nullptr};
    FileTransferModel *m_fileTransferModel{nullptr};

    QComboBox      *m_conversationSelector{nullptr};
    QLineEdit      *m_toUriEdit{nullptr};
    QLabel         *m_presenceIndicator{nullptr};
    QLabel         *m_typingIndicator{nullptr};
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
