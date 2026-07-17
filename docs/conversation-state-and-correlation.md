# Conversation State and Correlation (Task W112)

How `ConversationWorkspacePanel`/`ConversationListModel` derive each row's
live state, and what is/isn't correlated to what.

## Call state per conversation

`SipManager` is single-active-call (a pre-existing architectural
constraint — confirmed via `m_activeCall` usage throughout `SipManager`,
unrelated to this task). A conversation row's `callStateText` is therefore
only ever non-empty for **at most one row at a time**: the one whose
normalized peer URI matches `SipManager::activeCallRemoteUri()`. It is
never inferred from any other signal (button state, SDP presence, etc.) —
`refreshRows()` reads `SipManager::instance().callState()` /
`activeCallRemoteUri()` directly, live, on every `callStateChanged`.

**Not implemented, not applicable**: multiple simultaneous calls (to the
same or different peers) — there is no call-id-keyed list anywhere in
`SipManager` to build that on top of. This is the same documented
limitation as Task W111's file-transfer/messaging work.

## Presence per conversation

`row.presenceText` reads `PresenceStore::current(peerUri)` directly (the
*raw*, non-normalized peer URI — presence entity URIs are not run through
`ConversationModel::normalizePeer()` anywhere in the codebase today, so a
case-differing subscription could theoretically miss a match; noted as a
known limitation, not fixed in this task since `PresenceStore` predates
`ConversationModel` and changing its keying is out of this task's scope).
`SubscriptionState::Unknown` renders as "Not available" — never a
fabricated "Offline", matching the rule established in Task W111's
[client-presence-and-capabilities.md](client-presence-and-capabilities.md).

## Unread vs. IMDN Displayed — two different concepts, never conflated

- **Unread count** (`ConversationModel::unreadCountFor`/`markRead`) is a
  purely local, UI-level, session-only concept: "has the user looked at
  this conversation's list of messages." It has no wire representation.
- **IMDN Displayed** (`MessageHistoryEntry::displayNotificationRequested`/
  `displayedImdnSent`, Task W096) is a protocol-level notification sent
  back to the *peer* confirming their message was displayed — governed by
  `AppSettings::autoSendDisplayedImdn()` and
  `SipManager::sendDisplayedImdnForEntry()`.

Selecting a conversation calls `ConversationModel::markRead()` (local-only)
but does **not** trigger an IMDN Displayed report — that remains gated by
its own existing setting/action, unchanged by this task. Marking a
conversation "read" in the UI and telling the peer "displayed" over SIP are
deliberately independent: a user could have "auto-send Displayed" off and
still want their own unread badge to clear on viewing.

## Actual-transport text per conversation

`row.actualTransportText` is read from `ConversationModel::lastMessageFor()`
— only populated when that entry is outbound and has
`actualTransport` set (Task W111's `MessageHistoryStore::updateTransportOutcome()`).
An inbound-only conversation (nothing sent yet) shows an empty transport
column rather than a stale/inherited value from a different conversation.
