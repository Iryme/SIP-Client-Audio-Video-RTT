#pragma once
#include <QObject>
#include <QString>

#include "sip/IsComposingInfo.h"
#include "sip/MessagingContentKind.h"

class ConversationModel;

// Owns the Client Messaging View's connection to the real messaging
// backend. Contains no protocol logic itself — every send/compose/IMDN/
// is-composing operation is delegated to the existing
// SipMessageComposer/SipManager/TypingIndicatorController exactly as
// MessagingDiagnosticsPage already does, so this class is presentation
// glue, not a second implementation.
class ClientMessagingController : public QObject
{
    Q_OBJECT
public:
    explicit ClientMessagingController(QObject *parent = nullptr);

    ConversationModel *conversationModel() const { return m_conversationModel; }

    struct SendOptions
    {
        QString toUri;
        QString body;
        MessagingContentKind contentType{MessagingContentKind::PlainText};
        bool requestImdn{false};
    };

    // Composes (SipMessageComposer::compose) and sends (SipManager::
    // sendSipMessage) opts.body to opts.toUri. The actual transport (SIP
    // MESSAGE vs MSRP vs fallback) is decided entirely inside
    // SipManager::sendSipMessage/MessagingTransportPolicy from the current
    // global AppSettings::messagingTransportMode() — this method never
    // second-guesses that decision or sends on two transports itself.
    // Returns false (with error set) if SIP MESSAGE is disabled
    // (AppSettings::enableSipMessage()) or composition/send is rejected.
    bool sendMessage(const SendOptions &opts, QString &error);

    // Forwards to this conversation's own TypingIndicatorController (never a
    // shared/global one — see ConversationModel::typingControllerFor).
    void notifyComposingTextChanged(const QString &peerUri, bool nonEmpty);
    void stopComposing(const QString &peerUri);

    // Task W096: manual "mark as read" action for an inbound entry.
    bool markDisplayed(qint64 inboundEntryId, QString &error);

private slots:
    void onTypingSendRequested(const QString &peerUri, IsComposingInfo::State state, int refreshSeconds);

private:
    ConversationModel *m_conversationModel{nullptr};
};
