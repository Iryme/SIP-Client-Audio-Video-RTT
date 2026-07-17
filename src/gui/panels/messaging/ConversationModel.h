#pragma once
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include "sip/IsComposingInfo.h"
#include "sip/MessageHistoryEntry.h"

class TypingIndicatorController;

// Groups the flat MessageHistoryStore list (Task W093 design: one store, no
// per-peer bucketing) by normalized peer URI for the Client Messaging View.
// This is new aggregation code, not a duplicate of the store — it never
// writes to MessageHistoryStore, only reads/re-emits its signals filtered by
// conversation. Also owns one TypingIndicatorController per conversation
// (never shared across peers), so composing in one conversation can never
// start or expire a typing session belonging to another.
class ConversationModel : public QObject
{
    Q_OBJECT
public:
    explicit ConversationModel(QObject *parent = nullptr);

    // Case-insensitive key used to group entries into a conversation. Peer
    // URIs already arrive pre-normalized from SipManager/SipCall in the
    // overwhelming majority of cases; this only folds case so "Alice@X" and
    // "alice@x" are treated as the same conversation.
    static QString normalizePeer(const QString &peerUri);

    // Distinct peers seen so far, oldest-first by first appearance.
    QStringList conversationPeers() const;

    // Chronological history for one conversation (a filtered snapshot of
    // MessageHistoryStore, not a separate copy kept in sync manually).
    QList<MessageHistoryEntry> historyFor(const QString &peerUri) const;

    // Lazily creates (once per peer) and returns this conversation's typing
    // controller. Same instance for the lifetime of this ConversationModel.
    TypingIndicatorController *typingControllerFor(const QString &peerUri);

    // The peer's most recently observed remote typing state
    // ("active"/"idle"/"gone"), or empty if never seen.
    QString remoteTypingState(const QString &peerUri) const;

    // Task W112 (Conversation Workspace). Most recent entry in this
    // conversation (default-constructed, id == 0, if none) and its
    // timestamp — used for a list row's preview/last-activity ordering.
    MessageHistoryEntry lastMessageFor(const QString &peerUri) const;
    QDateTime lastActivityFor(const QString &peerUri) const;

    // Number of inbound entries in this conversation strictly newer than
    // the last read marker set by markRead(). A conversation never read
    // this session has every inbound entry counted as unread. This is a
    // session-only, in-memory concept — MessageHistoryStore itself is not
    // persisted across restarts, so a read cursor referencing its ids
    // would be meaningless after one; it intentionally does not survive
    // an app restart.
    int unreadCountFor(const QString &peerUri) const;

    // Marks every entry currently in this conversation as read (moves the
    // cursor to the latest entry id seen so far). Never affects any other
    // conversation's cursor.
    void markRead(const QString &peerUri);

signals:
    // Emitted whenever MessageHistoryStore appends/updates an entry
    // belonging to this conversation, so the UI can refresh just that
    // conversation instead of re-scanning the whole store.
    void conversationUpdated(const QString &peerUri);
    void conversationListChanged();

    // Re-emitted from this conversation's TypingIndicatorController with the
    // peer attached (the controller's own signal doesn't carry one, since
    // it's per-instance already) — the actual SIP MESSAGE compose/send for
    // this is done by ClientMessagingController, not here.
    void typingSendRequested(const QString &peerUri, IsComposingInfo::State state, int refreshSeconds);

private slots:
    void onEntryAppended(const MessageHistoryEntry &entry);
    void onEntryUpdated(const MessageHistoryEntry &entry);

private:
    void noteEntry(const MessageHistoryEntry &entry);

    QHash<QString, TypingIndicatorController *> m_typingControllers;
    QStringList m_knownPeers; // normalized keys, first-seen order
    QHash<QString, qint64> m_lastReadEntryId; // normalized peer -> last-read MessageHistoryEntry::id
};
