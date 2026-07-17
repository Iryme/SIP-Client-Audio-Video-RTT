# ConversationModel (Task W111, extended Task W112)

`ConversationModel` (`src/gui/panels/messaging/ConversationModel.{h,cpp}`)
groups `MessageHistoryStore`'s flat, non-bucketed entry list (its original
Task W093 design — one store, no per-peer bucketing) by normalized peer
URI. It is read-only aggregation: it never writes to `MessageHistoryStore`,
only reads `snapshot()` and re-emits its signals filtered by conversation.

## API (as of Task W112)

```cpp
static QString normalizePeer(const QString &peerUri);   // case-fold + trim

QStringList conversationPeers() const;                    // first-seen order
QList<MessageHistoryEntry> historyFor(const QString &peerUri) const;

TypingIndicatorController *typingControllerFor(const QString &peerUri);
QString remoteTypingState(const QString &peerUri) const;

// Task W112 additions:
MessageHistoryEntry lastMessageFor(const QString &peerUri) const;
QDateTime lastActivityFor(const QString &peerUri) const;
int unreadCountFor(const QString &peerUri) const;
void markRead(const QString &peerUri);

signals:
    void conversationUpdated(const QString &peerUri);
    void conversationListChanged();
    void typingSendRequested(const QString &peerUri, IsComposingInfo::State, int refreshSeconds);
```

## One instance per Clients page, shared

There is exactly one `ConversationModel` instance for the whole Clients
page, owned by `ClientMessagingController` (constructed in
`ClientMessagingView`'s constructor). `ConversationWorkspacePanel` reads
the *same* instance via `ClientMessagingController::conversationModel()`
(exposed through a new `ClientMessagingView::controller()` accessor) —
never a second, duplicate model. This is the concrete mechanism behind the
task rule "no duplicating MessageHistoryStore."

## Per-conversation isolation

- **History**: `historyFor()` filters by normalized peer per call — no
  separate per-peer copy to desynchronize.
- **Typing**: one `TypingIndicatorController` per peer
  (`typingControllerFor()`), created lazily, cached for the model's
  lifetime — composing in one conversation can never start or expire a
  typing session belonging to another
  (`tests/test_conversation_model.cpp::typingControllerForIsIndependentAcrossPeers`).
- **Unread**: `m_lastReadEntryId` is a `QHash<peer, entryId>` — marking one
  conversation read never touches another's cursor
  (`markReadDoesNotAffectOtherConversations`).

## Unread tracking: session-only by design

`unreadCountFor()` counts inbound entries whose `MessageHistoryEntry::id`
(a monotonically increasing counter, store-wide, not per-peer) is greater
than the peer's last-read cursor. `markRead()` advances that cursor to the
latest entry id seen for that peer.

This cursor is **never persisted**. `MessageHistoryStore` itself resets on
every app restart (it has no `QSettings`/file-backed persistence — pure
in-memory, bounded list). Persisting a read cursor that references ids
from a store that won't exist after a restart would be meaningless, so the
cursor is a plain in-memory `QHash` member, not an `AppSettings` key. This
is documented as an intentional design choice, not an oversight — contrast
with the *pinned* flag (`AppSettings::pinnedConversationPeers()`), which
**is** persisted because it's contact-level metadata independent of
message history, analogous to `ContactStore`.

## Determinism (`markRead` guard)

`markRead()` is a no-op if the computed cursor doesn't actually advance —
this isn't just an optimization: `ClientMessagingView` calls `markRead()`
both when a conversation becomes selected *and* whenever
`conversationUpdated` fires for the currently-viewed conversation (so new
messages arriving while already viewing never accumulate as unread).
Without the no-op guard, `markRead()`'s own `conversationUpdated` emission
would immediately re-trigger that same handler — an infinite loop. The
guard (skip emitting when the cursor doesn't change) makes the recursion
terminate deterministically after exactly one step.
