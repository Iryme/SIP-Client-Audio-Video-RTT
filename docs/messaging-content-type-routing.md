# Messaging Content-Type Routing (Task W113F)

How an inbound messaging payload — over either SIP MESSAGE or MSRP — gets
classified and routed so protocol control payloads (IMDN reports,
is-composing notifications, CPIM envelopes) never render as a chat bubble
in the Client Messaging view, while still being fully visible in Tools.

## The bug this fixes

Two separate inbound paths existed with different (and both incomplete)
classification:

- **Plain SIP MESSAGE** (`SipManager::onAccountInstantMessageReceived`)
  detected `message/imdn+xml` and `application/im-iscomposing+xml` and
  routed them correctly — but had **no `message/cpim` branch at all**. A
  CPIM-wrapped message fell straight through to `appendInbound()` with the
  raw CPIM envelope (headers + blank line + wrapped body) as its stored
  body — rendered verbatim as a chat bubble.
- **MSRP** (the `SipCall::msrpPayloadReceived` handler in `SipManager.cpp`)
  didn't classify anything at all — every MSRP payload, including CPIM,
  IMDN, and is-composing, went straight into `appendInbound()` as a plain
  message. A separate class, `MsrpPayloadDispatcher`, already implemented
  the correct unwrap-and-classify logic and had its own passing test suite
  (`tests/test_msrp_payload_dispatcher.cpp`) — but nothing in the
  production receive path ever called it. Several docs (`imdn.md`,
  `is-composing.md`, `msrp-foundation.md`) claimed MSRP reused it; that was
  aspirational, not real.

## The fix: one shared routing function

`SipManager::routeInboundMessagingPayload()` (private, in `SipManager.cpp`)
is now the single place both paths call:

```
onAccountInstantMessageReceived(SIP MESSAGE)  ─┐
                                                ├─> routeInboundMessagingPayload()
SipCall::msrpPayloadReceived (MSRP)          ──┘
```

Steps, in order:

1. **Unwrap CPIM first.** If `MessagingContentKindDetector::detect(contentType)
   == Cpim`, call `CpimParser::parse(body)`. If it parses (`cpim.present`
   and a non-empty inner `Content-Type`), the *inner* Content-Type and body
   replace the outer ones for every step below — so a CPIM-wrapped IMDN or
   is-composing notification (both legal per RFC 5438/CPIM-carrying
   conventions) gets classified the same as an unwrapped one. If CPIM
   parsing fails, the payload becomes a placeholder row
   (`MessageHistoryStore::appendInboundUnsupported()`) and routing stops —
   the raw envelope is never stored or rendered.
2. **IMDN** (`message/imdn+xml`, unwrapped or not): `ImdnParser::parse()`,
   append via `MessageHistoryStore::appendInboundImdn()` (its own row,
   `isImdnReport=true`), correlate delivery state onto the *outbound*
   entry the report refers to via `correlateDelivery()`. Never triggers a
   further IMDN of its own.
3. **is-composing** (`application/im-iscomposing+xml`, unwrapped or not):
   `IsComposingParser::parse()`, append via
   `MessageHistoryStore::appendInboundTyping()` (`isTypingNotification=true`).
4. **Everything else** (plain text, HTML, or CPIM successfully unwrapped to
   one of those): normal `MessageHistoryStore::appendInbound()`, with
   auto-IMDN-report sending applied exactly as before.

## Where the Client UI actually excludes these rows

Routing correctly is necessary but not sufficient — `MessageHistoryStore`
is a flat, unfiltered append-only list by design (Tools' Message History
table intentionally shows everything). The exclusion for the *Client*'s
chat bubble list happens one layer up:

- `MessageHistoryEntry::isProtocolEvent()` — `true` for `isImdnReport`,
  `isTypingNotification`, or `isUnsupportedOrMalformed`.
- `ConversationModel::userVisibleHistoryFor(peerUri)` — `historyFor()`
  filtered to exclude `isProtocolEvent()` rows. `ClientMessagingView::
  reloadHistory()` uses this (not `historyFor()`) to populate the visible
  history list.
- `ConversationModel::lastMessageFor()`/`unreadCountFor()` also use
  `userVisibleHistoryFor()` — an IMDN report or typing notification can
  never become a conversation's preview text or count toward its unread
  badge (`ConversationWorkspacePanel` reads both of these).
- `ConversationModel::historyFor()` itself stays **unfiltered** — it's
  still used internally by `remoteTypingState()` (needs the typing rows to
  exist) and is available for any future Tools-side consumer that wants
  every row, protocol events included.

## Content types NOT changed by this task

- `application/pidf+xml` (Presence) never touched `MessageHistoryStore` in
  the first place — it goes straight to `PresenceStore` via
  `SipAccount::buddyPresenceChanged`/`onAccountBuddyPresenceChanged`,
  already cleanly separated. No change needed.
- MSRP REPORT / MSRP SEND response were already handled correctly —
  `SipCall::msrpDeliveryStatusChanged` only ever *updates* an existing
  outbound entry's `deliveryState` via `correlateMsrpDelivery()`; it never
  creates a new history row, so it never risked becoming its own bubble.

## Known limitation

CPIM-wrapped Message-ID / Disposition-Notification headers (distinct from
the plain-SIP-MESSAGE header fields of the same name) are not extracted —
`CpimInfo` only models `From`/`To`/`DateTime`/`Subject`/`Content-Type`. A
CPIM-wrapped plain message today gets auto-IMDN-report behavior only via
the outer SIP MESSAGE's own `Message-ID`/`Disposition-Notification`
headers (unaffected by CPIM unwrap, since those come from SIP headers, not
the CPIM envelope) — this is correct for the common case but means a peer
that puts its own Message-ID *inside* the CPIM envelope instead of the SIP
header won't correlate. Not fixed in this task (would require extending
`CpimParser`/`CpimInfo` with an assumption about wire format this project
hasn't confirmed).
