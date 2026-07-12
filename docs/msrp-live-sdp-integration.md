# Live MSRP SDP Integration (Task W101)

> **Task W102 extension**: this document covers only the *offerer* path
> (this client's own INVITE/re-INVITE/UPDATE). The *answer* path — replying
> to a peer-initiated `m=message` offer — is covered separately in
> [msrp-offer-answer.md](msrp-offer-answer.md), including why it needed a
> different injector function (`answerMessageMediaAtIndex`, replacing a
> section in place rather than appending).

Supersedes the "`Call::onCallSdpCreated` is read-only" conclusion in
`docs/msrp-foundation.md` §0 (Task W100). That conclusion was incorrect —
this task re-audited the vendored pjproject source directly (not just
public headers) and found a real, safe, public-API injection path.

## Audit (Phase 1)

- **`OnCallSdpCreatedParam::sdp.pjSdpSession` is live and mutable.**
  `SdpSession::fromPj` (`.deps/pjproject/pjsip/src/pjsua2/call.cpp:113-124`)
  sets `pjSdpSession = (void *)&sdp` — a raw pointer to the exact
  `pjmedia_sdp_session *sdp` that `pjsua_media_channel_create_sdp()` built,
  and that PJSIP uses downstream to construct the outgoing INVITE/answer.
  It is not a copy.
- **`Endpoint::on_call_sdp_created` only re-parses `wholeSdp`, never
  `pjSdpSession`.** (`.deps/pjproject/pjsip/src/pjsua2/endpoint.cpp:1547-1585`)
  It snapshots `wholeSdp` before the callback, and after the callback does a
  **string comparison**: only if the text changed does it
  `pjmedia_sdp_parse()` + `pj_memcpy()` a reparsed struct back over `sdp`.
  If `wholeSdp` is left untouched, nothing overwrites a direct mutation of
  `*(pjmedia_sdp_session*)pjSdpSession`.
- **`wholeSdp` is capped at `PJSUA2_MAX_SDP_BUF_LEN`** (1024 bytes,
  `.deps/pjproject/pjsip/include/pjsua2/config.hpp`) and already comes back
  empty for some real audio+video+RTT SDPs (see the existing comment at
  `src/sip/SipCall.cpp:608-609` from before this task). This makes the
  `wholeSdp` text round-trip an unreliable injection path even before MSRP
  is added.

**Decision: mutate `pjSdpSession` directly, never touch `wholeSdp`, never
edit vendored pjproject source.** This satisfies the task's rule 1 ("nu
modifica pjproject dacă există o soluție prin API-ul public") — the splice
uses only the public `pjmedia/sdp.h` API (`pjmedia_sdp_attr_create`,
`PJ_POOL_ZALLOC_T`, direct struct field assignment), the same pattern
PJSIP's own `pjsua_media.c` uses internally to append media sections (see
its SRTP crypto-duplication and "disabled media clone" code paths).

Memory: the dialog's own pool (used for `pjsua_media_channel_create_sdp`)
is not exposed to pjsua2's `OnCallSdpCreatedParam` (traced through
`pjsua_call.c`: `dlg->pool` for outgoing INVITEs, `rdata->tp_info.pool` for
incoming/answers — neither reachable from the callback). Instead, one
`pj_pool_t` per `SipCall` is created via the public `pjsua_pool_create()`
API, lives for the whole call, and is released in `~SipCall()`. Mixing pool
ownership within one `pjmedia_sdp_session` is safe and is exactly what
PJSIP itself does.

## Implementation (Phase 2)

`MsrpSipMediaInjector::injectMessageMedia()` (`src/msrp/MsrpSipMediaInjector.{h,cpp}`)
builds a `pjmedia_sdp_media` for `m=message <port> TCP[/TLS]/MSRP *` with
`a=path`/`a=setup`/`a=accept-types`/`a=accept-wrapped-types` attributes and
appends it to `sdp->media[sdp->media_count++]`. `port == 0` (listener
failed to bind) omits the negotiation attributes — a valid RFC 3264
rejected section, never an invalid/unreachable offer.

`SipCall::Impl::PjCall::onCallSdpCreated` calls this **only on the offerer
path** — detected via `prm.remSdp` being empty, which pjsua2 guarantees
only when this side is generating the offer (see
`OnCallSdpCreatedParam`'s doc comment in `call.hpp`). This covers the
initial INVITE and any re-INVITE/UPDATE this client itself originates.

**Answering a peer-initiated `m=message` offer is not yet implemented.**
PJSUA's own handling of media types it doesn't recognize (audio/video/text
are the only types it tracks internally) needs a separate audit before an
automatic accept/reject splice at the matching media index can be trusted
not to corrupt the SDP or violate RFC 3264's "answer every offered m= line
at the same index" rule. Detection of the peer's own `m=message` offer
still works (`MsrpSipIntegration::detectFromSdp`, unchanged from W100) —
this client just doesn't yet act on it automatically.

## Listener-before-offer (Phase 3)

`SipCall::Impl::ensureMsrpSessionForOutgoingSdp()` runs at the top of the
offerer branch, before injection: it lazily creates this call's own
`MsrpSession`, calls `listenAsPassive()` (a synchronous `QTcpServer::listen`
bind), and only then reads `transportLocalPort()` for the SDP. Idempotent —
a re-INVITE/UPDATE later in the same call reuses the same listener rather
than opening a second one. On bind failure, the offer falls back to
`m=message 0 ...` instead of advertising a port nobody could reach.

All values (bind address, port mode, TLS, accept-types, timeouts) come from
`AppSettings` (already introduced in W100) — never hardcoded. The
advertised host prefers `msrp/advertisedHost` if configured, else reuses
the same connection address PJSIP already resolved for this SDP's
audio/video media (`MsrpSipMediaInjector::resolveSessionConnectionHost`),
never a literal.

## Proof (wire-bytes test)

`tests/test_msrp_sip_media_injector.cpp` parses a real base SDP (audio +
video, exactly as PJSIP would produce it) with `pjmedia_sdp_parse()`,
injects via the same code path `SipCall` uses, and re-serializes with
`pjmedia_sdp_print()` — then asserts the **literal wire text** contains
`m=message <port> TCP/MSRP *` plus the expected attributes, that
audio/video lines are untouched, and that TLS/port-0 variants behave
correctly. This is direct evidence about transmitted bytes, not the
internal `MsrpSessionInfo` model. `ctest -R test_msrp_sip_media_injector`.

## Known limitations

- Answering a peer-offered `m=message` is not implemented (see above).
- No live capture against a real SIP-Server-RTT/Blink/AG Projects/Linphone
  session exists in this environment — see
  `docs/agent-results/W101-live-msrp-sip-integration-result.md` for what
  was and wasn't executed.
