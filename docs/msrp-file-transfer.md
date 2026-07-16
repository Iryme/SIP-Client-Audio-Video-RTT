# MSRP File Transfer — RFC 5547 (Task W104)

Builds directly on the MSRP protocol stack from
[msrp-foundation.md](msrp-foundation.md) — no new transport, no new
session lifecycle. A file transfer is an ordinary MSRP message (same
`MsrpSession`/`MsrpMessageChunker`/`MsrpChunkAssembler` pipeline) whose
`Content-Disposition: attachment` header and (at the SDP level)
`a=file-selector`/`a=file-disposition`/`a=file-transfer-id` attributes mark
it as a file rather than a chat message.

## What was implemented

| Piece | File | Responsibility |
|---|---|---|
| `file-selector` attribute | `src/msrp/MsrpFileSelector.h/.cpp` | Pure parse/build of the RFC 5547 `a=file-selector` SDP attribute value (`name`/`size`/`type`/`hash`); also the one sanctioned place a peer-supplied file name is reduced to a safe display basename |
| `Content-Disposition` frame header | `src/msrp/MsrpFrame.h`, `MsrpFrameParser.cpp`, `MsrpFrameSerializer.cpp` | Parsed/serialized exactly like `Content-Type` — repeated on every chunk (Task W104 extension of the existing header set) |
| Chunker/assembler carry-through | `MsrpMessageChunker`, `MsrpChunkAssembler` | `contentDisposition` is threaded through outbound chunk-building and inbound reassembly, same as `contentType` was already |
| Send | `MsrpSession::sendFile()` | Reads a file fully into memory (bounded by the existing `maxMessageBytes` cap — rejects oversized files rather than truncating/streaming-partial), computes a SHA-1 digest, builds `Content-Disposition: attachment; filename="…"` with a sanitized name, sends via the existing chunked-SEND pipeline |
| Receive (detection) | `MsrpSession::fileTransferReceived` signal | Fires *in addition to* the existing `payloadReceived` signal (never instead of it) whenever an assembled message's `Content-Disposition` type is `attachment` |
| Receive (save) | `src/msrp/MsrpFileReceiver.h/.cpp` | Pure, caller-triggered: atomic disk write (`QSaveFile` — either the whole file lands or nothing does) to a caller-chosen path, plus SHA-1/SHA-256 hash verification against a negotiated `file-selector` hash |
| UI | `src/gui/panels/MsrpPage.h/.cpp` | "Send File…" button (file picker) on the existing manual test session; a save dialog is offered automatically on `fileTransferReceived`, pre-filled with the sanitized suggested name |

> **Task W111 update**: `MsrpPage`'s Send File was manual-test-session-only,
> disconnected from a live call's own `MsrpSession`. `SipCall` gained
> `sendMsrpFile()`/`msrpFileTransferReceived` forwarding to/from its own
> session (mirroring the pre-existing `sendMsrpMessage()`/
> `msrpPayloadReceived` pattern), and `SipManager::sendMsrpFile()`/
> `msrpFileTransferReceived` expose it further. The new, explicitly
> **experimental** Client Messaging View (see
> [client-messaging-workspace.md](client-messaging-workspace.md)) is the
> first consumer of this live-call path — a "Save Received File…" action
> replaces `MsrpPage`'s automatic save-dialog-on-receive with an explicit
> user click before anything touches disk. No changes to
> `MsrpSession`/`MsrpFileSelector`/`MsrpFileReceiver`/the chunker/assembler
> themselves.

## Why the in-memory model, not disk streaming

The existing MSRP pipeline (established in W100) already builds every
outbound message's SEND frames from a single in-memory `QByteArray`
(`MsrpMessageChunker::buildSendFrames`) and reassembles every inbound
message into a single in-memory `QByteArray`
(`MsrpChunkAssembler::feedChunk`), bounded by a hard `maxMessageBytes` cap
that a peer can never exceed before any large allocation happens. File
transfer reuses that same, already-proven bound rather than introducing a
separate streaming-to-disk I/O path — which would be a much larger
architectural change with no interoperability partner available to
validate it against (see "What was not tested" below). `sendFile()` simply
rejects (returns `ok=false`) files larger than the configured cap instead
of silently truncating or partially sending.

## Path safety

Two independent rules keep a peer's file name from ever being used as a
literal filesystem path:

1. `MsrpSession::sendFile()` — the *outbound* filename in
   `Content-Disposition` is always `QFileInfo(filePath).fileName()` (never
   the caller-supplied full path), further sanitized by
   `MsrpFileSelector::sanitizeFileNameForDisplay()`.
2. On the *inbound* side, `fileTransferReceived`'s `suggestedFileName` is
   the sanitized basename only — `sanitizeFileNameForDisplay()` strips any
   `/`/`\` directory components and rejects `.`/`..`. `MsrpFileReceiver::
   saveToPath()` never derives a path from peer data itself; the save path
   is always the caller's own choice (a `QFileDialog` result in
   `MsrpPage`, or a test fixture path in the unit tests).

## Content-Disposition parsing (receive side)

`MsrpSession::handleFrame()` reads only the disposition *type* token (the
part before the first `;`) to decide "is this a file transfer" —
parameters after `;` (e.g. `filename="..."`) are extracted separately and
only ever used through `sanitizeFileNameForDisplay()`. An unrecognized or
absent `Content-Disposition` means `fileTransferReceived` simply does not
fire for that message; `payloadReceived` (the pre-existing signal) still
fires exactly as before — no change to any existing chat-message code
path.

## Tests

- `test_msrp_file_selector` — `file-selector` parse/build round-trip,
  malformed/missing-field tolerance, filename sanitization (path
  traversal, `.`/`..`, empty).
- `test_msrp_file_receiver` — atomic save (`QSaveFile`), failure on an
  unwritable path leaves no partial file, SHA-1 hash verification
  (match/mismatch/empty-expected/unknown-algorithm).
- `test_msrp_session_harness` gained `sendsAndReceivesFileTransfer` (real
  loopback TCP send/receive of a 3000-byte file, chunked, asserts the SHA-1
  returned by `sendFile()` and the bytes/filename delivered via
  `fileTransferReceived` match) and `sendFileRejectsMissingFile`.

## What was not tested (honest limitations)

- **No live interoperability**: exactly as documented in
  [msrp-live-interoperability.md](msrp-live-interoperability.md), there is
  no SIP-Server-RTT instance or third-party MSRP client reachable from this
  environment. File transfer against a real peer is `NOT RUN` — only the
  local loopback harness (this client talking to itself) was exercised.
- **No resumable/partial transfer**: RFC 5547 does not itself define
  resume semantics, and none is implemented here — a failed transfer must
  be restarted from the beginning.
- **No progress reporting UI**: `sendFile()` sends all chunks in one call
  (same fire-and-forget pattern `sendMessage()` already used); there is no
  per-chunk progress signal. `MsrpDiagnosticsStore`'s existing per-frame
  diagnostics log still shows each chunk as it is sent/received.
- **File transfer is not wired into any live SIP call** — same limitation
  as MSRP generally (§0 of msrp-foundation.md): only the MSRP page's manual
  test session can currently send/receive a real file, exactly as it could
  already send/receive a real chat message.
