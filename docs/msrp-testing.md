# MSRP Testing (Task W100)

> **Task W101 addition**: `test_msrp_sip_media_injector` (only built when
> `ENABLE_PJSIP=ON`) proves live SDP injection at the wire-bytes level —
> see [msrp-live-sdp-integration.md](msrp-live-sdp-integration.md). It
> parses a real base SDP with `pjmedia_sdp_parse()`, injects via the exact
> code path `SipCall` uses, and asserts the `pjmedia_sdp_print()` output
> (not an internal model) contains the expected `m=message` bytes.
>
> **Task W102 additions**: `test_msrp_sip_media_injector` gained 3 more
> cases proving the *answer* path (`answerMessageMediaAtIndex`) at the same
> wire-bytes level — see [msrp-offer-answer.md](msrp-offer-answer.md). A new
> `test_trace_comparator` target (8 tests, no PJSIP dependency) covers the
> client/server trace comparator — see
> [client-server-trace-comparison.md](client-server-trace-comparison.md).
>
> **Task W107 addition**: RFC 4976 MSRP relay authentication testing
> (`test_msrp_digest_auth`, `test_msrp_relay_client`,
> `test_msrp_session_relay_transport`) — a separate, additive test surface
> with no shared code path with anything above, since relay support is not
> wired into the live call flow. See
> [msrp-relay-testing.md](msrp-relay-testing.md).
> `test_windows_trace_json_export` gained a case for the new schemaVersion 3
> `msrpSessions` fields. 68/68 CTest targets pass, zero regressions.
>
> **Task W104 additions**: two new targets, `test_msrp_file_selector` (RFC
> 5547 `file-selector` attribute parse/build) and `test_msrp_file_receiver`
> (atomic save + hash verification) — see
> [msrp-file-transfer.md](msrp-file-transfer.md).
> `test_msrp_session_harness` gained `sendsAndReceivesFileTransfer` and
> `sendFileRejectsMissingFile`. 71/71 CTest targets pass, zero regressions.

## Automated unit/protocol tests

| Test target | Covers |
|---|---|
| `test_msrp_path` | URI/path parsing (msrp/msrps, IPv4/IPv6/hostname, invalid scheme/port/session-id), session-id generation |
| `test_msrp_sdp_negotiator` | `m=message` parsing (order-independent, multiple sections, port-0 rejection, accept-types/wrapped-types, direction), setup role negotiation (active/passive/actpass/holdconn, invalid active+active/passive+passive), offer-block construction |
| `test_msrp_frame_parser` | SEND/REPORT/response parsing, fragmented headers/body/delimiter, multiple frames per read, NUL/high bytes/CRLF in body, invalid transaction-id, unknown/duplicate headers, oversized frame, recovery after invalid frame, Byte-Range, continuation flags |
| `test_msrp_frame_serializer` | SEND/REPORT/response serialization, binary body round-trip, header-injection rejection, exact CRLF/delimiter, Byte-Range |
| `test_msrp_chunker` | Single/multi-chunk splitting, Byte-Range correctness, final `$` vs. intermediate `+`, report headers only on final chunk, unique transaction-ids per chunk |
| `test_msrp_chunk_assembler` | Single-chunk fast path, in-order multi-chunk reassembly, duplicate-chunk idempotency, gap/overlap detection, abort, size-limit rejection, stale-message purge |
| `test_msrp_transaction_store` | Queued→Accepted, REPORT success/failure correlation, unknown transaction-id no-op, pending-count exclusion of terminal states, clear |
| `test_messaging_transport_policy` | All four modes' initial-transport decisions, post-failure fallback rules (including MsrpRequired's "never fallback") |
| `test_msrp_payload_dispatcher` | text/plain, text/html, CPIM-unwrapped text, IMDN correlation, is-composing, binary-content-not-added-to-history, unknown-but-text-like-content-added |
| `test_msrp_session_harness` | Real client(active)↔server(passive) exchange over 127.0.0.1 (see below); Task W104: also a real file transfer + missing-file rejection |
| `test_msrp_file_selector` | RFC 5547 `file-selector` attribute parse/build round-trip, malformed/missing-field tolerance, filename sanitization |
| `test_msrp_file_receiver` | Atomic disk save (no partial file on failure), SHA-1 hash verification (match/mismatch/empty/unknown-algorithm) |

Plus the full existing suite (W090–W099) runs unchanged as the regression
gate.

## Local integration harness (`test_msrp_session_harness`)

Two `MsrpSession` instances in the same test process, communicating over
real `QTcpSocket`/`QTcpServer` loopback connections on `127.0.0.1` with an
OS-assigned ephemeral port (never a hardcoded port) — no dependency on any
real MSRP relay/server, safe to run under `ctest`. Covered scenarios:

- Active connects to a local passive listener; both sides reach
  `Established`.
- `SEND` of a plain-text message, verified received byte-for-byte on the
  other side (`payloadReceived` signal).
- A chunked message (body larger than `msrpChunkSizeBytes`) is split,
  transmitted as multiple `SEND` chunks, and correctly reassembled.
- Graceful connection close is observed as a state transition.

**Not covered by the current harness** (documented gap, not silently
omitted): a dedicated `REPORT`/`Success-Report` round-trip assertion
(the mechanism is implemented and exercised implicitly whenever
`msrpRequestReports` is on, but no test asserts the REPORT frame's exact
content), an `abort` (`#`) mid-transfer scenario, TLS-over-loopback (would
require a locally-generated test certificate/key — not set up in this
task), and deliberate invalid-frame injection against a live socket (frame
parser invalid-input handling is covered directly in
`test_msrp_frame_parser` without a socket, which is the higher-value
place to test it).

## Manual interoperability test with SIP-Server-RTT

Placeholders only — never a real server/user/relay:

1. Enable MSRP (experimental) in the client: Enable MSRP, pick a transport
   policy (Automatic or MSRP preferred), enable TCP (and TLS if the
   relay/server supports it).
2. Configure the SIP profile `<profile>` against `<server-host>`.
3. Initiate a session that would carry `m=message` per SIP-Server-RTT's
   MSRP relay documentation (`<msrp-relay-host>`), e.g. a messaging dialog
   to `sip:<user>@<server-host>`.
4. Verify:
   - the SDP offer/answer actually negotiated `m=message` (visible via
     `MsrpSipIntegration`'s detection — the MSRP page's session table
     should show a `Detected` entry for the call's Call-ID);
   - setup roles resolve without an active/active or passive/passive
     conflict;
   - paths parse correctly;
   - **note**: this client does not currently establish a live MSRP TCP/TLS
     connection automatically from that negotiated SDP (see
     [msrp-foundation.md](msrp-foundation.md) §0/§12) — a full end-to-end
     SEND/response/REPORT exchange against the real relay was not
     attempted in this session and is not claimed to work automatically;
   - the MSRP page's own manual test session (§ above) can be pointed at
     `<msrp-relay-host>` directly to exercise a real SEND/response/REPORT
     exchange against the relay, independent of the SIP call.
5. Export the Interop JSON and confirm `msrpSessions`/`msrpEvents` appear
   for whatever was exercised.
6. Test the SIP MESSAGE fallback path (set transport policy to
   "SIP MESSAGE only" or trigger an MSRP failure) and confirm messaging
   continues to work via the pre-existing SIP MESSAGE path, unaffected by
   MSRP being enabled.
7. Confirm rejecting/disabling MSRP for a call does not affect the
   audio/video/RTT media in that same call (independent m= sections).

**This scenario was not executed against a live SIP-Server-RTT instance in
this session** (no server available) — the local harness (above) is what
was actually run and verified. No interoperability claim beyond "the local
harness passes" and "SDP detection was validated against synthetic SDP
text in unit tests" should be inferred from this document.
