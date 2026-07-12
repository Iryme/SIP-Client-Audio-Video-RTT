# Task-W104 — MSRP File Transfer (agent prompt, condensed)

## Repository / Branches

`SIP-Client-Audio-Video-RTT`. Branch chain (must be executed strictly in
order): `feature/w103-lmpe-foundation` →
**`feature/w104-msrp-file-transfer`** →
`feature/w105-advanced-msrp-hardening`. No merge into main/release/other
integration branches without explicit request.

## Scope

Add RFC 5547 MSRP file transfer on top of the existing MSRP protocol stack
(Task W100–W102): send/receive an actual file over an established
`MsrpSession`, negotiated file metadata (`file-selector`), and a safe
receive-side save path — without inventing a new transport, session
lifecycle, or in-memory-bound model beyond what W100 already established.

## Permanent rules (unchanged across all tasks)

No hardcoded IPs/domains/hostnames/ports/users/passwords/paths; never break
REGISTER/audio/video/RTT/SIP MESSAGE/CPIM/IMDN/is-composing/Presence/XCAP/
diagnostics/SIP Ladder/existing exports; never block the UI thread; never
touch UI directly from a socket callback; protocol data via `QByteArray`;
never log/export secrets or full sensitive paths; never claim
interoperability unless actually tested (`PASS`/`FAIL`/`BLOCKED`/`NOT RUN`
only); never modify the SIP-Server-RTT repository from here (it does not
exist in this workspace — see Task W103's audit); separate branch,
thematic commits, prompt/result docs, `docs/project-status.md` update, full
build, automated tests, push, no merge.

## What was actually done

1. Added `Content-Disposition` as a proper `MsrpFrame` header field
   (parsed/serialized exactly like `Content-Type`, threaded through
   `MsrpMessageChunker`/`MsrpChunkAssembler`) — the standing mechanism RFC
   5547 uses to mark a message as `attachment` rather than an ordinary
   chat message.
2. Added `MsrpFileSelector` (pure parse/build of the SDP `a=file-selector`
   attribute value, plus the one sanctioned peer-filename sanitizer).
3. Added `MsrpSession::sendFile()` (bounded in-memory read, same
   `maxMessageBytes` cap as every other MSRP message, SHA-1 digest,
   sanitized `Content-Disposition: attachment; filename="..."`, reuses the
   existing chunked-SEND pipeline) and a new `fileTransferReceived` signal
   that fires additively alongside the pre-existing `payloadReceived` (zero
   change to any existing chat-message/CPIM/IMDN/is-composing code path).
4. Added `MsrpFileReceiver` (atomic `QSaveFile` write, SHA-1/SHA-256
   verification) — the disk write always happens at a caller-chosen path,
   never derived from peer data.
5. Wired a minimal, additive UI: "Send File…" button and an automatic
   save-on-receive dialog on the MSRP page's existing manual test session.
6. New tests: `test_msrp_file_selector`, `test_msrp_file_receiver`, plus
   `sendsAndReceivesFileTransfer`/`sendFileRejectsMissingFile` in the
   existing `test_msrp_session_harness` (real loopback TCP send/receive).
7. No live interoperability test was run — same documented limitation as
   every MSRP task since W100 (no SIP-Server-RTT instance or third-party
   MSRP client reachable from this environment).

See `docs/agent-results/W104-msrp-file-transfer-result.md` and
`docs/msrp-file-transfer.md` for the full report.
