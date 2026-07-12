# MSRP Foundation (Task W100)

A complete, independently-testable MSRP (RFC 4975/4976) protocol stack for
the Windows client: SDP negotiation model, path parsing, session lifecycle,
frame parser/serializer, TCP/TLS transport, transactions, chunking, and
payload dispatch reusing the existing CPIM/IMDN/is-composing infrastructure.
LMPE (Task W101) is explicitly out of scope. MSRP is disabled by default and
never activates without explicit configuration.

## 0. Architectural decision (mandatory pre-implementation audit)

**PJSIP/pjsua2 MSRP support: none.** A case-insensitive search across every
header in `.deps/pjsip-msvc-install/include` (pjsip, pjmedia, pjsua2) for
"msrp" returns zero matches. This vendored pjproject build has no
`pjmedia-transport-msrp`, no `pjsip-simple` MSRP module, nothing — at
neither the C API nor the pjsua2 C++ wrapper layer.

**SDP offer/answer construction is pjsua2's high-level `Call` API, not
manual text.** `SipCall::makeCallWithOptions` calls `pjCall->makeCall()`,
which auto-generates the SDP offer from whatever media transports are
registered (audio/video/RTT). There is no manual SDP-string-building
touchpoint to hook an `m=message` line into on the way out.

**`Call::onCallSdpCreated` is read-only.** `pj::OnCallSdpCreatedParam::sdp`
is a `SdpSession` struct with a `wholeSdp` string field and a raw
`pjSdpSession` void* — but `SdpSession` has no `toPj()` counterpart to
`fromPj()`; editing `wholeSdp` (a C++ copy) does not feed back into the
actual outbound SDP pjsua2 sends. The only way to influence the real SDP
would be to reach through the raw `pjSdpSession` pointer into pjsip's
memory-pool-owned C struct — exactly the kind of fragile, undocumented
manual manipulation this project's conventions already reject (see Task
W098's Publish decision and Task W099's Digest decision for the same
"no fragile manual SIP transactions" principle).

**Decision**: MSRP is implemented as a complete, standalone protocol stack
built on Qt (`QTcpSocket`/`QSslSocket`) — SDP negotiation is done via pure
text parsing/construction (`MsrpSdpNegotiator`), entirely independent of
pjsua2's media pipeline. **Live outbound SDP injection into a real INVITE
is not implemented** (there is no safe hook for it in this pjproject
version) — what IS implemented is read-only **detection**: the existing,
unmodified `onCallSdpCreated`/SDP-logging hook in `SipCall.cpp` now also
calls `MsrpSipIntegration::detectFromSdp()` (one additional line, never
touching `prm.sdp`), so `m=message` appearing in any real call's SDP is
surfaced in `MsrpSessionStore`/the MSRP page — without any risk to
audio/video/RTT, since the hook is purely observational.

A full, real MSRP session (SEND/RECEIVE, actual TCP/TLS transport) can
today be exercised via the MSRP page's manual test session (user/config-
supplied host/port/path) or the local test harness — both fully functional
and tested — but not automatically from a live call's negotiated SDP. What
full live-call wiring would require is described in §"What remains for
W101" below.

## 1. Architecture — separated layers

No monolithic class. Each concern is its own file:

| Layer | Class | Responsibility |
|---|---|---|
| SDP negotiation | `MsrpSdpNegotiator` | Parses/builds `m=message` sections (pure text) |
| Path | `MsrpPath` / `MsrpUri` | `msrp(s)://` URI parse/build, session-id generation |
| Session model | `MsrpSessionInfo` | Plain data — the only type the UI binds to |
| Session state machine | `MsrpSession` | Orchestrates transport+parser+assembler+transactions |
| Transport (abstract) | `MsrpTransport` | Common async connect/listen/send/close interface |
| TCP transport | `MsrpTcpTransport` | `QTcpSocket`/`QTcpServer`-based |
| TLS transport | `MsrpTlsTransport` | `QSslSocket`-based |
| Frame model | `MsrpFrame` / `MsrpByteRange` | Plain struct, binary-safe body |
| Frame parser | `MsrpFrameParser` | Incremental, binary-safe |
| Frame serializer | `MsrpFrameSerializer` | Exact CRLF/delimiter, injection-safe |
| Chunking (outbound) | `MsrpMessageChunker` | Pure, splits body into SEND chunks |
| Chunking (inbound) | `MsrpChunkAssembler` | Reassembles by Message-ID, bounded memory |
| Transactions | `MsrpTransaction` / `MsrpTransactionStore` | SEND/REPORT/response correlation |
| Session registry | `MsrpSessionStore` | Global singleton, UI-facing |
| Diagnostics | `MsrpDiagnosticsEvent` / `MsrpDiagnosticsStore` | Global singleton, frame log |
| Payload dispatch | `MsrpPayloadDispatcher` | Reuses CpimParser/ImdnParser/IsComposingParser |
| Transport policy | `MessagingTransportPolicy` | Pure decision engine (§M) |
| Live-call detection | `MsrpSipIntegration` | Read-only SDP observation (§0) |

## 2. Session lifecycle

`MsrpSessionState`: `Disabled → Detected → Offered → Answered → Negotiated
→ Connecting → Connected → Established → Disconnecting → Closed / Failed`.

Deliberately never collapses these: `MsrpSipIntegration::detectFromSdp()`
only ever reaches `Detected` (an `m=message` line was observed in real call
SDP — no transport exists). `MsrpSession` (the manual test session / local
harness) drives `Connecting → Connected → Established` only once a real
`QTcpSocket`/`QSslSocket` has actually connected — `Established` is never
set just because SDP negotiation looked complete.

## 3. SDP negotiation

`MsrpSdpNegotiator::parseMessageBlocks()` finds every `m=message` section
(order-independent attributes: `a=path`, `a=accept-types`,
`a=accept-wrapped-types`, `a=setup`, `a=connection`, `a=sendrecv`/
`a=sendonly`/`a=recvonly`/`a=inactive`, `a=file-selector`,
`a=file-disposition`, `a=file-transfer-id`), tolerates multiple `m=message`
sections, and flags a `port=0` line as `rejected` (not fatal) — matching
"reject media, don't reject the whole SDP."

`negotiateRole(localSetup, remoteSetup)` implements RFC 4145 offer/answer
rules: `active+passive`/`passive+active` → valid connector/listener roles;
`active+active`/`passive+passive` → explicit error, never silently picks
one; `holdconn` on either side → `HoldConn` role (no connection attempted);
an unresolved `actpass` on either side after offer/answer → explicit error
("negotiation not complete"), never a default connect attempt.

`buildOfferBlock()` generates a `m=message` section from a caller-supplied
`MsrpUri` (host/port/session-id always from runtime/config, never
hardcoded) — used by tests and by the manual test session, not by any live
INVITE (see §0).

## 4. MSRP path / session-id

`MsrpPath::parseUri()`/`parsePath()` handle `msrp://`/`msrps://`, IPv4,
bracketed IPv6, hostnames, explicit/implicit ports, and the mandatory
`;transport` parameter — rejecting empty host/session-id/transport and
unsupported schemes. `generateSessionId()` produces a 16-character
(96-bit) cryptographically random, URL-safe (`QRandomGenerator`) id —
never derived from user/host identifiers, never reused across sessions.

## 5. Frame model, parser, serializer

`MsrpFrame`/`MsrpByteRange` are plain structs; the body is always
`QByteArray`, never routed through `QString` (protocol bytes are never
"text" — CRLF/NUL/high bytes inside a body are preserved exactly).

`MsrpFrameParser` is incremental and binary-safe: it scans the raw
accumulation buffer with `QByteArray::indexOf` (never a full-buffer
text-conversion search), handles a start-line split across multiple
`feed()` calls, a body containing CRLF/NUL/bytes ≥0x80, multiple frames in
one `feed()` call, and unknown/duplicate headers (last-one-wins, preserved
for diagnostics, never re-interpreted). Results are one of `NeedMoreData` /
`Complete` / `Invalid` / `LimitExceeded`; after an `Invalid`/`LimitExceeded`
result the parser searches for the next plausible `"MSRP "` start-line and
discards everything before it, so a single malformed frame never
permanently wedges the connection.

`MsrpFrameSerializer` produces exact CRLF/delimiter bytes and validates
every header value for embedded `\r`/`\n`/NUL before writing anything —
rejecting (empty output, `ok=false`) rather than ever emitting an injected
extra header/line.

## 6. SEND, responses, REPORT

`MsrpSession::handleFrame()` responds to every well-formed inbound `SEND`
chunk with a `200`/`400` response, and — only once the chunked message is
fully reassembled and the final chunk requested `Success-Report: yes` —
emits a `REPORT` with `Status: 000 200 OK`. Inbound `REPORT`s are
correlated to the originating outbound transaction by Message-ID via
`MsrpTransactionStore`. `MsrpTransactionStatus` explicitly separates
`Accepted` (a 200 response to the SEND itself) from `ReportedSuccess`/
`ReportedFailure` (the REPORT) — these are never conflated with each other
or with an IMDN delivered/displayed report, which lives one layer up in
`MessageHistoryStore`/`ImdnParser` and is populated only via
`MsrpPayloadDispatcher` once a `message/imdn+xml` payload is actually
received *inside* an MSRP SEND body.

## 7. Chunking / Byte-Range

`MsrpMessageChunker::buildSendFrames()` (pure, outbound) splits a body into
`msrpChunkSizeBytes`-sized chunks, each with its own random transaction-id,
correct `Byte-Range: start-end/total`, and `+`/`$` continuation —
`Success-Report`/`Failure-Report` are only set on the final chunk.

`MsrpChunkAssembler` (inbound) reassembles by Message-ID: single-chunk
messages complete immediately; multi-chunk messages are buffered
(pre-sized once `total` is known), with duplicate-chunk detection
(idempotent, warning-only), gap/overlap detection (buffered with a
warning — full adversarial-reordering correctness is a documented
limitation, see docs/msrp-protocol.md), and a hard `maxMessageBytes` cap
enforced *before* any large allocation — a peer can never force unbounded
memory growth. `purgeStale()` lets `MsrpSession` evict a message that never
completes within `msrpTransactionTimeoutMs`.

## 8. Payload dispatch — no duplicated parsers

`MsrpPayloadDispatcher::dispatch()` reuses, unmodified: `CpimParser::parse`,
`ImdnParser::parse`, `IsComposingParser::parse`,
`MessagingContentKindDetector::detect`, and
`MessageHistoryStore::appendInbound`/`appendInboundImdn`/
`appendInboundTyping`/`correlateDelivery` — the exact same calls
`SipManager::onAccountInstantMessageReceived` already makes for SIP
MESSAGE, just fed from `MsrpSession::payloadReceived` instead. CPIM is
unwrapped first (inner Content-Type re-classified); IMDN reports correlate
delivery state onto the original outbound Message-ID; is-composing updates
the existing typing-indicator history rows. An unrecognized Content-Type is
only added to Message History if it plausibly is text (conservative binary
sniff: no NUL bytes, <5% non-printable) — genuinely binary content stays
diagnostics-only, per requirement L.

## 9. Transport selection / fallback

`MessagingTransportPolicy` (pure decision engine, `src/msrp/
MessagingTransportPolicy.h/.cpp`): `SipMessageOnly` always uses SIP
MESSAGE; `MsrpRequired` uses MSRP only if established, otherwise an
explicit error (never a silent fallback); `MsrpPreferred`/`Automatic` use
MSRP if established, else SIP MESSAGE fallback if
`allowSipMessageFallback` is on, else an explicit error.
`decideFallbackAfterMsrpFailure()` is the only path that permits a
post-failure retry on SIP MESSAGE, and still refuses for `MsrpRequired`.
**Not yet wired into the live compose-and-send path** (see §"What remains
for W101") — the policy itself is fully implemented and unit-tested, but
`SipManager`'s actual send call still always uses SIP MESSAGE.

## 10. Call integration

See §0: only read-only detection is wired into `SipCall::onCallSdpCreated`.
Audio/video/RTT SDP generation is completely untouched — the added code
path never mutates `prm.sdp`, so there is no way it can affect the
generated offer.

## 11. UI, ladder, diagnostics, export

New "MSRP" nav page (`src/gui/panels/MsrpPage.h/.cpp`) — configuration,
a manual test session (experimental, independent of live calls), the
active-sessions table (fed by `MsrpSessionStore`, so both manual test
sessions and `MsrpSipIntegration`-detected live-call sessions appear),
diagnostics frame log, and JSON/TXT export buttons.

MSRP is plain-TCP protocol traffic, not SIP — per the task's explicit
requirement it is **not** drawn into the SIP Ladder as SIP traffic; its own
diagnostics table on the MSRP page is the equivalent surface.

`InteropTraceExporter` gained `msrpSessions` (from `MsrpSessionStore`) and
`msrpEvents` (from `MsrpDiagnosticsStore`) top-level arrays, plus four
additive fields on every existing `events` entry (`selectedTransport`/
`actualTransport`/`fallbackUsed`/`fallbackReason`) — see
`docs/windows-trace-json-export.md` for the full field list and the
explicit "why schemaVersion stays 2" note.

## 12. What remains for W101 / future work

- **Live SDP injection into a real INVITE/re-INVITE is not implemented**
  (§0) — would require either a pjproject rebuild with native MSRP
  support, or directly manipulating the raw `pjmedia_sdp_session` C struct
  through pjsua2's `pjSdpSession` pointer (assessed as too fragile/risky
  for this task's "no fragile manual SIP transactions" rule).
- `MessagingTransportPolicy` is implemented and tested but not yet called
  from `SipManager`'s actual send path — sending still always uses SIP
  MESSAGE regardless of the configured transport mode.
- Chunk reassembly for adversarial out-of-order/overlapping chunks is
  best-effort (buffered + warned), not RFC-exhaustive.
- LMPE encode/decode over MSRP (Task W101, explicitly out of scope here).
- TLS server-side certificate/key provisioning for `listenAsPassive()` is
  usable for the local test harness but has no production
  certificate-configuration UI yet.
