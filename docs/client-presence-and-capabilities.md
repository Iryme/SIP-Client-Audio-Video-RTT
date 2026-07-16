# Client Presence and Capabilities (Task W111)

## Presence

`ClientMessagingView` binds directly to `PresenceStore::presenceUpdated`/
`PresenceStore::current(peerUri)` (Task W098) — no new presence logic, no
duplicated PIDF parsing. Display mapping:

| `PresenceInfo` state | Client view shows |
|---|---|
| `SubscriptionState::Unknown` (never subscribed/no data) | "Not available" |
| Any known subscription state | `"<extendedStatus> (<subscriptionState>)"`, e.g. `"Away (active)"` |

`ExtendedStatus::Offline` is only ever shown when the store actually holds a
PIDF-derived Offline state — never fabricated when presence simply isn't
configured or the peer doesn't support it (that case is "Not available"),
per the task's explicit "never show Offline when the real state is Unknown"
rule.

## Capabilities

`ClientMessagingView::refreshCapabilities()` derives, on demand rather than
caching a separate model object:

- **MSRP session state** for the current peer: only considered when
  `SipManager::activeCallRemoteUri()` matches the selected conversation, by
  scanning `MsrpSessionStore::snapshot()` for a session whose
  `sipHeaderCallId` equals `SipManager::activeCallSipId()` — never inferred
  from IP/port or from the peer URI alone.
- **Send File availability**: enabled only when that session's
  `isEstablished()` is true.
- **Remote typing state**: `ConversationModel::remoteTypingState()`, the
  most recent inbound `isTypingNotification` entry for that peer in
  `MessageHistoryStore`.

This intentionally avoids a separate "capabilities" cache/model object
that could drift from the real session/store state — every refresh reads
the actual current state directly.

## Multi-conversation isolation

- History: `ConversationModel::historyFor()` filters `MessageHistoryStore`
  by normalized peer URI per call — no separate per-peer copy to
  desynchronize.
- Typing: one `TypingIndicatorController` per peer
  (`ConversationModel::typingControllerFor()`), created lazily, never
  shared — see `tests/test_conversation_model.cpp`
  `typingControllerForIsIndependentAcrossPeers`.
- File transfer: the "Save Received File" offer is scoped to the peer it
  actually arrived from (`m_pendingFilePeer`) and hidden when a different
  conversation is selected, rather than a single view-wide slot that could
  offer to save the wrong peer's file after switching conversations.
- **Not applicable in this app**: true concurrent calls to the same or
  different peers, since `SipManager` supports exactly one active call at a
  time (pre-existing architecture, unrelated to this task). Conversation
  *history* isolation works regardless of call state; MSRP *session*
  correlation only ever has one live session to match against.
