# Client Messaging Transport Selection (Task W111)

## Enum reused verbatim

`ClientMessagingView`'s transport selector uses `MessagingTransportMode`
(`src/msrp/MsrpTypes.h`, Task W100 section M) directly — no parallel enum:

- `Automatic`
- `SipMessageOnly` (labeled "SIP MESSAGE" in the UI)
- `MsrpPreferred` ("MSRP Preferred")
- `MsrpRequired` ("MSRP Required")

Wire strings (`automatic`/`sip-message-only`/`msrp-preferred`/
`msrp-required`) are the same ones `MsrpPage`'s existing transport-policy
combo already reads/writes.

## Persistence: global, not per-conversation

Backed by the existing `AppSettings::messagingTransportMode()`/
`setMessagingTransportMode()` — **global** across all conversations, not
scoped per peer/profile. This was a deliberate choice (the task explicitly
asks to "choose whichever fits the current architecture and document it"):
no per-peer settings store exists anywhere in this codebase today, and
`MsrpPage` already treats this setting as global. Adding per-conversation
persistence would have meant introducing a new settings-storage mechanism
solely for this one preference, which the task's own "don't duplicate
persistence machinery" rule argues against. If per-conversation transport
preference becomes a real requirement later, it should be layered on top of
`ConversationModel` (which already has a natural per-peer keying point),
not by changing `AppSettings`' global key.

## Decision ownership

The transport selector only sets *policy* (`AppSettings`); it never decides
which transport a specific message actually uses. That decision is made
once, inside `SipManager::sendSipMessage()` via
`MessagingTransportPolicy::decideInitialTransport()`/
`decideFallbackAfterMsrpFailure()` — exactly the same call path
`MessagingDiagnosticsPage` and every other messaging surface in this app
already goes through. `ClientMessagingController::sendMessage()` composes
the message and calls `SipManager::sendSipMessage()`; it never second-guesses
or duplicates the transport decision itself, and never sends the same
message on two transports.

## Selected / actual / fallback display

`MessageHistoryEntry` gained `actualTransport`/`fallbackReason` (Task W111,
see [client-messaging-workspace.md](client-messaging-workspace.md)) so the
UI can show what actually happened, not just what was requested:

```
Selected: MSRP Preferred
Actual: sip-message-fallback
Fallback: MSRP send failed; fell back to SIP MESSAGE
```

`ClientMessagingView::refreshTransportStatus()` reads the most recent
outbound entry with `actualTransport` set for the current conversation and
renders it — it never shows a generic "sent" checkmark divorced from what
transport was really used.

## Content type and IMDN

Content-type selector (Plain/HTML/CPIM) maps to `MessagingContentKind`
exactly as `SipMessageComposer::Options::contentType` already expects; CPIM
wrapping is still gated by `AppSettings::enableCpim()` (a global feature
flag, independent of the per-message content-type choice — matching the
existing `cpimEnabled` semantics in `SipMessageComposer::Options`). No CPIM
body is ever hand-built in the UI. IMDN request checkboxes (Delivered/
Displayed) set `SipMessageComposer::Options::requestImdn`, reusing the
existing IMDN request-header generation — not a new implementation.
Content-type and IMDN-checkbox state persist via
`AppSettings::clientMessagingContentType()`/`clientMessagingRequestDelivered()`/
`clientMessagingRequestDisplayed()` (UI preference only, never a draft or
payload).
