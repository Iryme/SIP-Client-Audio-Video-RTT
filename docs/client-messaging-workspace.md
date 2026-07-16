# Client Messaging Workspace (Task W111)

Integrates SIP MESSAGE, MSRP, CPIM, IMDN, is-composing, Presence, and
(experimental) file transfer directly into the Clients page, instead of
requiring the technical Messaging Diagnostics / MSRP / Presence pages for
everyday use. Those technical pages are unchanged and still exist — now
grouped under [tools-navigation.md](tools-navigation.md) — this workspace is
a second, user-facing surface built on top of the same backend.

## Architecture

```
ClientMessagingView (QWidget, presentation only)
    |
ClientMessagingController (QObject, compose/send glue)
    |
ConversationModel (QObject, groups MessageHistoryStore by peer)
    |
MessageHistoryStore / MessagingTransportPolicy / PresenceStore /
TypingIndicatorController / SipManager / SipCall
```

No protocol logic is duplicated at any layer:

- `ClientMessagingController::sendMessage()` builds a `ComposedSipMessage`
  via `SipMessageComposer::compose()` and sends it via
  `SipManager::sendSipMessage()` — the exact same two calls
  `MessagingDiagnosticsPage` already made. The transport decision itself
  (SIP MESSAGE vs MSRP vs fallback) happens entirely inside
  `SipManager::sendSipMessage()`/`MessagingTransportPolicy`; the controller
  never second-guesses it.
- `ConversationModel` never writes to `MessageHistoryStore` — it only reads
  `snapshot()` and re-emits `entryAppended`/`entryUpdated` filtered by
  normalized peer URI. `MessageHistoryStore` itself stays a flat list (its
  original W093 design); grouping lives one layer up, in new code, not
  bolted onto the store.
- One `TypingIndicatorController` per conversation, owned by
  `ConversationModel` and created lazily via `typingControllerFor()` — never
  a single shared/global instance, so composing in one conversation can
  never start or expire a typing session belonging to another.

## Files

- `src/gui/panels/messaging/ConversationModel.{h,cpp}`
- `src/gui/panels/messaging/ClientMessagingController.{h,cpp}`
- `src/gui/panels/messaging/ClientMessagingView.{h,cpp}`
- `src/gui/panels/messaging/FileTransferModel.{h,cpp}` (experimental)

Inserted as a fourth column in `MainWindow::buildClientsPage()`, alongside
Contacts, Video, and RTT — see
[client-messaging-transport-selection.md](client-messaging-transport-selection.md)
for how it picks its target, and
[client-presence-and-capabilities.md](client-presence-and-capabilities.md)
for the Presence/capabilities display.

## Message History additions

`MessageHistoryEntry` gained three fields this task needed and didn't have
before: `actualTransport`, `fallbackReason`, `msrpMessageId`, set via the new
`MessageHistoryStore::updateTransportOutcome()`. This also closed a
pre-existing gap: `SipCall::msrpDeliveryStatusChanged` was previously only
logged, never correlated into Message History — it's now wired through the
new `MessageHistoryStore::correlateMsrpDelivery()`, a **separate id space**
from `correlateDelivery()`'s SIP Message-ID matching (MSRP Message-IDs and
SIP Message-IDs are never the same identifier space — see
[imdn.md](imdn.md)).

## File transfer (experimental)

`SipCall::sendMsrpFile()`/`msrpFileTransferReceived` forward to/from
`MsrpSession::sendFile()`/`fileTransferReceived` (Task W104) — a capability
that existed on `MsrpSession` but was previously unreachable from a live
call (`MsrpPage`'s Send File is a manual test-session action, disconnected
from `SipCall`). `FileTransferModel` wraps this in a thin
Idle/Sending/Sent/Failed state machine. `MsrpSession::sendFile()` reads a
file fully into memory and sends it as one MSRP message — there is no
chunk-level progress signal and no mid-transfer cancel hook anywhere in the
existing MSRP pipeline, so this is honestly reflected here rather than
faked: no progress bar, no Cancel button. Marked "Experimental" in the UI
and gated off whenever no MSRP session is established for the current peer
— it never silently falls back to SIP MESSAGE for a file.

## Known limitations

- **Single active call**: `SipManager` supports exactly one active call at a
  time (a pre-existing architectural constraint, not something this task
  changed). "Multiple simultaneous calls to the same peer" (Faza 14) is
  therefore not applicable in the current app — conversation/history
  isolation by peer URI is fully implemented and tested
  (`tests/test_conversation_model.cpp`), but there is only ever one live
  MSRP session at a time to correlate against.
- **Transport mode is global, not per-conversation** — see
  [client-messaging-transport-selection.md](client-messaging-transport-selection.md).
