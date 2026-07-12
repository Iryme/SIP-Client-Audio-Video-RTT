# MSRP Peer Association (Task W101 Phase 4, updated W102 Phase 4)

How this client correlates a live SIP dialog with its MSRP session,
without relying solely on IP/port/peer-URI.

> **Task W102 update**: `MsrpSessionInfo::role` (`ActiveConnector` /
> `PassiveListener`) is now always real — set by whichever of
> `MsrpSession::connectAsActive`/`listenAsPassive` was actually called,
> which itself now depends on the negotiated `a=setup` role for
> peer-initiated offers (see [msrp-offer-answer.md](msrp-offer-answer.md))
> rather than always being passive. `role` is exported in schemaVersion 3
> (see [windows-trace-json-export.md](windows-trace-json-export.md)) and
> shown in the MSRP page's session table. The "what is not yet implemented"
> section below (inbound connection validation against To-Path/session-id)
> is **still not implemented in W102** — see the final report's
> limitations section for why it was not prioritized this pass either.

## Identity fields

`MsrpSessionInfo` (`src/msrp/MsrpSessionInfo.h`) carries, per session:

- `sessionKey` — this app's own internal call id (`SipCall::callId()`,
  e.g. `"pjsip-3"`), the key `MsrpSessionStore` indexes on.
- `sipCallId` — same value as `sessionKey` today (set via
  `MsrpSession::setSipCallId`).
- `sipHeaderCallId` *(new, Phase 4)* — the real SIP `Call-ID` header
  (`pj::CallInfo::callIdString`), distinct from the two fields above.
  Populated right after this call's SDP is injected, from inside
  `onCallSdpCreated` (wrapped in `try/catch`: `getInfo()` can throw during
  a teardown race, in which case the field simply stays unset for that
  event rather than crashing).
- `mediaIndex` *(new, Phase 4)* — the position of this session's
  `m=message` section within the negotiated SDP (`sdp->media_count - 1`
  right after injection). Lets a call with multiple media sections (or a
  renegotiated media order across a re-INVITE) still identify which
  section is "this" MSRP session.
- `localSessionId` / `remoteSessionId` — from the MSRP URI's `session-id`
  component (already existed in W100).
- `localPath` / `remotePath` — the full `a=path` URI chains.

## Why IP/port alone isn't the correlation key

Each `SipCall` owns exactly one `MsrpSession`, and each `MsrpSession` binds
its own dedicated OS-assigned ephemeral TCP/TLS listener
(`MsrpTcpTransport::listenPassive` → `QTcpServer::listen(addr, 0)`, unless
`msrp/portMode` is explicitly set to `fixed` by the user). Because the
port is unique per session by construction, multiple simultaneous MSRP
sessions to the *same peer* (e.g. two separate calls) never collide on
port — each gets its own listener, its own session-id, its own
`sessionKey`. This satisfies the task's requirement ("trebuie permisă
existența simultană a mai multor sesiuni MSRP către același peer fără
coliziuni") structurally, without needing a shared-listener demultiplexer.

`MsrpTcpTransport::onNewConnection` stops listening after accepting the
first connection (`m_server->close()`), matching a passive-role session's
one-connection expectation — so there is no ambiguity about which
listener a given accepted socket belongs to either.

## What is *not* yet implemented

The task's longer-form peer-association diagnostic (matching an accepted
connection's first `SEND`'s `To-Path`/session-id against this session's
own `localSessionId` before treating frames as belonging to it, and
classifying mismatches as "no matching session" / "path mismatch" /
"session-id mismatch" / etc.) is **not implemented in this pass**. Given
the current one-listener-per-session design already prevents port-level
collisions, this validation is a hardening measure against a
misbehaving/malicious peer connecting to the right port with the wrong
session-id — valuable, but scoped out here in favor of the higher-priority
wire-transmission and transport-policy work. Left for a follow-up task.
