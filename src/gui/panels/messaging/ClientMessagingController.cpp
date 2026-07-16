#include "ClientMessagingController.h"
#include "ConversationModel.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "sip/SipManager.h"
#include "sip/SipMessageComposer.h"
#include "sip/SipProfileManager.h"
#include "sip/TypingIndicatorController.h"

ClientMessagingController::ClientMessagingController(QObject *parent)
    : QObject(parent)
    , m_conversationModel(new ConversationModel(this))
{
    connect(m_conversationModel, &ConversationModel::typingSendRequested,
            this, &ClientMessagingController::onTypingSendRequested);
}

bool ClientMessagingController::sendMessage(const SendOptions &opts, QString &error)
{
    if (!AppSettings::enableSipMessage()) {
        error = QStringLiteral("SIP MESSAGE is disabled in Settings");
        return false;
    }
    if (opts.toUri.trimmed().isEmpty()) {
        error = QStringLiteral("No recipient selected");
        return false;
    }
    if (opts.body.trimmed().isEmpty()) {
        error = QStringLiteral("Message is empty");
        return false;
    }

    SipMessageComposer::Options composerOpts;
    composerOpts.toUri = opts.toUri;
    const SipProfile activeProfile = SipProfileManager::instance().activeProfile();
    composerOpts.fromUri = activeProfile.isNull() ? QString() : activeProfile.effectiveSipUri();
    composerOpts.contentType = opts.contentType;
    composerOpts.body = opts.body;
    composerOpts.cpimEnabled = AppSettings::enableCpim();
    composerOpts.requestImdn = opts.requestImdn;

    const ComposedSipMessage composed = SipMessageComposer::compose(composerOpts);
    if (!composed.valid) {
        error = composed.error;
        return false;
    }

    const bool ok = SipManager::instance().sendSipMessage(composed, error);
    if (ok)
        stopComposing(opts.toUri);
    return ok;
}

void ClientMessagingController::notifyComposingTextChanged(const QString &peerUri, bool nonEmpty)
{
    if (auto *controller = m_conversationModel->typingControllerFor(peerUri))
        controller->onTextChanged(nonEmpty);
}

void ClientMessagingController::stopComposing(const QString &peerUri)
{
    if (auto *controller = m_conversationModel->typingControllerFor(peerUri))
        controller->stop();
}

bool ClientMessagingController::markDisplayed(qint64 inboundEntryId, QString &error)
{
    return SipManager::instance().sendDisplayedImdnForEntry(inboundEntryId, error);
}

void ClientMessagingController::onTypingSendRequested(const QString &peerUri,
                                                      IsComposingInfo::State state,
                                                      int refreshSeconds)
{
    if (!AppSettings::enableIsComposing() || !AppSettings::autoTypingNotifications())
        return;
    if (peerUri.trimmed().isEmpty())
        return;

    SipMessageComposer::IsComposingOptions opts;
    opts.toUri = peerUri;
    const SipProfile activeProfile = SipProfileManager::instance().activeProfile();
    opts.fromUri = activeProfile.isNull() ? QString() : activeProfile.effectiveSipUri();
    opts.state = state;
    opts.refreshSeconds = refreshSeconds;

    const ComposedSipMessage composed = SipMessageComposer::composeIsComposing(opts);
    if (!composed.valid)
        return;

    QString error;
    if (!SipManager::instance().sendSipMessage(composed, error)) {
        Logger::instance().warn(LogCategory::Sip,
            QStringLiteral("Client view: is-composing notification send failed: %1").arg(error));
    }
}
