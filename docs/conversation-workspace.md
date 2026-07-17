# Conversation Workspace (Task W112)

Inverts the Clients page's navigation model from call-centric to
Contact → Conversation → Messaging → Call: a conversation list
(`ConversationWorkspacePanel`) is now the primary interaction surface, and
starting a call is an action launched *from* a selected conversation, not
the reverse (the previous model: dial a number first, messaging bolted on
as a side column — see [client-messaging-workspace.md](client-messaging-workspace.md)
for that W111 baseline).

## Layout

`MainWindow::buildClientsPage()`'s left column now has two stacked pieces,
outside the pre-existing `QScrollArea` (a `QListView` needs its own
scrolling, not nested inside another scroll area):

```
leftContainer (QVBoxLayout)
├── ConversationWorkspacePanel   (stretch 2 — primary)
└── leftScroll                   (stretch 1 — unchanged: Call Control,
                                   Dialpad, ContactsPanel)
```

The call-control column itself (dialpad, status cards, video, RTT) is
**not** redesigned by this task — that is W113's job ("Call Workspace").
This task only inverts which surface is primary and wires a call action
into the existing controls.

## Row derivation

`ConversationListModel` rows are the union of:

1. `ContactStore::contacts()` — existing, QSettings-backed saved contacts.
2. `ConversationModel::conversationPeers()` (Task W111) — peers with at
   least one message this session.

Deduplicated by `ConversationModel::normalizePeer()`. A saved contact with
zero messages still gets a row (empty preview, "Contact → Conversation"
realized even before any message exists).

Per row: display name (contact name, else bare URI), last-message preview
+ timestamp, unread count, presence text, remote typing text, actual
transport of the last outbound message, call-state text (only when this
peer is the currently active call — see
[conversation-state-and-correlation.md](conversation-state-and-correlation.md)),
and a pinned flag.

## Sorting and filtering

`ConversationFilterProxyModel`: text filter over display name / peer URI /
last-message preview (same search-box idiom `CallHistoryPanel` already
uses); sort order is pinned rows first, then most-recent-activity first
— both encoded in a single `lessThan()` override, not two separate passes.

## Actions

- **Select a row** → `conversationSelected(peerUri)` → `MainWindow` sets
  `m_clientsTargetInput` and `ClientMessagingView::setPeerUri()` — the
  exact same two things the old inline conversation combo drove, so no
  other wiring needed to change.
- **Call button** → `callRequested(peerUri)` → `SipManager::makeCall()`,
  the same guarded call path `CallHistoryPanel::redialRequested` already
  uses (rejected if a call is already active — single-active-call is a
  pre-existing constraint, see
  [client-messaging-workspace.md](client-messaging-workspace.md#known-limitations)).
- **Pin toggle** → `AppSettings::setConversationPinned()`.

## Automation IDs

The `messagingContactSelector` id (Task W111 spec) moved from
`ClientMessagingView`'s now-removed inline combo to this panel's
`QListView` — it's the real conversation/contact selector now. New ids:
`conversationSearchEdit`, `conversationPinBtn`, `conversationCallBtn`.
