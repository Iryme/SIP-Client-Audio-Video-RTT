#include "ConversationModel.h"

#include "sip/MessageHistoryStore.h"
#include "sip/TypingIndicatorController.h"

#include <algorithm>

ConversationModel::ConversationModel(QObject *parent)
    : QObject(parent)
{
    connect(&MessageHistoryStore::instance(), &MessageHistoryStore::entryAppended,
            this, &ConversationModel::onEntryAppended);
    connect(&MessageHistoryStore::instance(), &MessageHistoryStore::entryUpdated,
            this, &ConversationModel::onEntryUpdated);

    // Seed from whatever history already exists (e.g. this ConversationModel
    // was constructed after messages were already exchanged this session).
    const auto existing = MessageHistoryStore::instance().snapshot();
    for (const MessageHistoryEntry &e : existing)
        noteEntry(e);
}

QString ConversationModel::normalizePeer(const QString &peerUri)
{
    return peerUri.trimmed().toLower();
}

void ConversationModel::noteEntry(const MessageHistoryEntry &entry)
{
    const QString key = normalizePeer(entry.peerUri);
    if (key.isEmpty())
        return;
    if (!m_knownPeers.contains(key)) {
        m_knownPeers.append(key);
        emit conversationListChanged();
    }
}

void ConversationModel::onEntryAppended(const MessageHistoryEntry &entry)
{
    const bool wasKnown = m_knownPeers.contains(normalizePeer(entry.peerUri));
    noteEntry(entry);
    Q_UNUSED(wasKnown)
    emit conversationUpdated(normalizePeer(entry.peerUri));
}

void ConversationModel::onEntryUpdated(const MessageHistoryEntry &entry)
{
    emit conversationUpdated(normalizePeer(entry.peerUri));
}

QStringList ConversationModel::conversationPeers() const
{
    return m_knownPeers;
}

QList<MessageHistoryEntry> ConversationModel::historyFor(const QString &peerUri) const
{
    const QString key = normalizePeer(peerUri);
    QList<MessageHistoryEntry> result;
    const auto all = MessageHistoryStore::instance().snapshot();
    for (const MessageHistoryEntry &e : all) {
        if (normalizePeer(e.peerUri) == key)
            result.append(e);
    }
    return result;
}

QList<MessageHistoryEntry> ConversationModel::userVisibleHistoryFor(const QString &peerUri) const
{
    QList<MessageHistoryEntry> result;
    for (const MessageHistoryEntry &e : historyFor(peerUri)) {
        if (!e.isProtocolEvent())
            result.append(e);
    }
    return result;
}

TypingIndicatorController *ConversationModel::typingControllerFor(const QString &peerUri)
{
    const QString key = normalizePeer(peerUri);
    if (key.isEmpty())
        return nullptr;
    auto it = m_typingControllers.find(key);
    if (it != m_typingControllers.end())
        return it.value();
    auto *controller = new TypingIndicatorController(this);
    connect(controller, &TypingIndicatorController::sendIsComposingRequested,
            this, [this, key](IsComposingInfo::State state, int refreshSeconds) {
        emit typingSendRequested(key, state, refreshSeconds);
    });
    m_typingControllers.insert(key, controller);
    return controller;
}

QString ConversationModel::remoteTypingState(const QString &peerUri) const
{
    const auto history = historyFor(peerUri);
    for (auto it = history.crbegin(); it != history.crend(); ++it) {
        if (it->isTypingNotification)
            return it->typingState;
    }
    return QString();
}

MessageHistoryEntry ConversationModel::lastMessageFor(const QString &peerUri) const
{
    const auto history = userVisibleHistoryFor(peerUri);
    return history.isEmpty() ? MessageHistoryEntry() : history.last();
}

QDateTime ConversationModel::lastActivityFor(const QString &peerUri) const
{
    return lastMessageFor(peerUri).timestamp;
}

int ConversationModel::unreadCountFor(const QString &peerUri) const
{
    const QString key = normalizePeer(peerUri);
    const qint64 lastRead = m_lastReadEntryId.value(key, 0);
    int count = 0;
    for (const MessageHistoryEntry &e : userVisibleHistoryFor(peerUri)) {
        if (e.direction == MessageHistoryEntry::Direction::Inbound && e.id > lastRead)
            ++count;
    }
    return count;
}

void ConversationModel::markRead(const QString &peerUri)
{
    const QString key = normalizePeer(peerUri);
    if (key.isEmpty())
        return;
    const qint64 previousCursor = m_lastReadEntryId.value(key, 0);
    qint64 latestId = previousCursor;
    for (const MessageHistoryEntry &e : historyFor(peerUri))
        latestId = std::max(latestId, e.id);
    if (latestId == previousCursor)
        return; // no-op: avoids an infinite loop if a caller re-marks-read
                // in response to conversationUpdated (e.g. a view that marks
                // its own currently-open conversation read on every update)
    m_lastReadEntryId.insert(key, latestId);
    emit conversationUpdated(key);
}
