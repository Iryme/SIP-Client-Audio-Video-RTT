# MSRP Protocol Layer (Task W100)

Covers `MsrpFrame`, `MsrpFrameParser`, `MsrpFrameSerializer`,
`MsrpMessageChunker`, `MsrpChunkAssembler`, `MsrpTransaction`/
`MsrpTransactionStore`. See [msrp-foundation.md](msrp-foundation.md) for
the overall architecture and [msrp-transport.md](msrp-transport.md) for
the transport layer these build on.

## Frame model

`MsrpFrame` (`src/msrp/MsrpFrame.h`) — plain struct: `isRequest`,
`transactionId`, `method` (request) / `responseCode`+`responseComment`
(response), `toPath`/`fromPath`/`messageId`/`contentType`/
`successReport`/`failureReport`/`status`, `byteRange`
(`MsrpByteRange{start,end,total}`, `-1` encodes RFC 4975's `*`),
`unknownHeaders` (preserved verbatim, never re-interpreted), `body`
(`QByteArray`, never `QString`), `continuation` (`+`/`$`/`#`).

## Parser (`MsrpFrameParser`)

Incremental and binary-safe (`feed(QByteArray) -> QList<Result>`,
`Result::status ∈ {NeedMoreData, Complete, Invalid, LimitExceeded}`):

1. Finds the start-line's terminating CRLF via `QByteArray::indexOf` (never
   a QString conversion of the whole buffer).
2. Splits `"MSRP <transaction-id> <method-or-status>"`, validates every
   transaction-id byte against an RFC-4975-compatible character class
   (alnum + `. - + % =`).
3. Searches for the end-line delimiter `"\r\n-------" + transaction-id`
   *from that specific transaction-id* — this is why the start-line must
   be parsed first.
4. Everything between the start-line's CRLF and the delimiter's leading
   CRLF is split at the first `"\r\n\r\n"` into headers vs. body (no blank
   line found ⇒ no body, matching frames with no `Content-Type`).
5. Header lines are parsed by first `:`; known headers populate typed
   fields, everything else (including malformed/duplicate headers) goes
   into `unknownHeaders` (last-one-wins, tolerated, never fatal).
6. The continuation flag byte and its trailing CRLF are validated; only
   `+`/`$`/`#` are accepted.

Tested against: fragmented headers, fragmented body, a delimiter split
across `feed()` calls, multiple frames in one `feed()` call, bodies
containing NUL bytes / CRLF / bytes ≥ 0x80, invalid transaction-ids,
oversized frames (`LimitExceeded`, checked *before* unbounded buffering),
and recovery after an invalid frame (the parser scans forward for the next
plausible `"MSRP "` start-line and discards everything before it, so one
malformed frame never permanently wedges the stream).

## Serializer (`MsrpFrameSerializer`)

Produces exact CRLF/delimiter bytes; every header value (including
`unknownHeaders`) is checked for embedded `\r`/`\n`/NUL before anything is
written — if any value would allow header/line injection, `serialize()`
returns an empty `QByteArray` with `ok=false` rather than emitting
anything. The body is appended verbatim (never round-tripped through
`QString`), verified round-trip-safe for arbitrary binary content
(0x00–0xFF) in `test_msrp_frame_serializer.cpp`.

## Chunking

**Outbound** (`MsrpMessageChunker::buildSendFrames`, pure): splits a body
into `chunkSizeBytes`-sized pieces when it exceeds that size; each chunk
gets its own randomly-generated transaction-id, a correct
`Byte-Range: start-end/total` header, and `+`/`$` continuation (only the
final chunk is `$`). `Success-Report`/`Failure-Report` headers are only
set on the final chunk — intermediate chunks always send `no` for both,
since RFC 4975 correlates reports to the whole message via Message-ID, not
per-chunk.

**Inbound** (`MsrpChunkAssembler::feedChunk`): a single-chunk message
(no `Byte-Range`, or a `1-N/N` range with `$`) completes immediately with
no buffering. A multi-chunk message is buffered in a pre-sized
`QByteArray` once `total` is known from the first Byte-Range with a
non-`*` total; **duplicate** chunks (same `start` seen twice) are silently
ignored with a warning (idempotent); **gap**/**overlap** are detected
(`start != data.size()+1`) and buffered anyway with a warning rather than
rejected outright — full RFC-perfect handling of adversarial reordering
(e.g. a `*`-total message with gaps) is a documented limitation, not
implemented at this foundation stage. `maxMessageBytes` is enforced
*before* any large allocation (checked against both the declared `total`
and the running `data.size()`), so a peer can never force unbounded memory
growth. `purgeStale(deadline)` lets the owning `MsrpSession` evict
messages that never complete within `msrpTransactionTimeoutMs`.

## Transactions

`MsrpTransactionStatus`: `Queued → Sending → AwaitingResponse → Accepted
→ ReportedSuccess` / `ReportedFailure`, or `TimedOut` / `Aborted` /
`Failed` at any point. `MsrpTransactionStore` is a plain, instantiable
(not global-singleton) store — one per `MsrpSession` — with a bounded
`kMaxRetainedCompleted` (1000) tail so long-lived sessions don't
accumulate unbounded completed-transaction history.

**Deliberately distinct, never conflated**: an MSRP `200` response to a
`SEND` (transaction `Accepted`) vs. a `REPORT`'s `Status: 000 200 OK`
(`ReportedSuccess`) vs. an IMDN `delivered`/`displayed` disposition (a
*payload*, delivered inside a SEND body, handled one layer up by
`MsrpPayloadDispatcher`/`ImdnParser`/`MessageHistoryStore`). These three
concepts live at three different protocol layers and are represented by
three different types in this codebase.
