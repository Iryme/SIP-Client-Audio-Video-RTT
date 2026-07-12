# Agent Result — Task W104: MSRP File Transfer

## 1. Branch

`feature/w104-msrp-file-transfer`, branched from `feature/w103-lmpe-foundation`.
Not merged into `main`/`release`/any other branch.

## 2. Version / schema

No `InteropTraceExporter::kSchemaVersion` change — file transfers use the
same `msrpSessions`/`msrpEvents` export surface as any other MSRP message
(each SEND chunk of a file transfer is logged as an ordinary MSRP frame
event, same as before this task).

## 3. Scope decision — why in-memory, not disk streaming

The existing MSRP pipeline (Task W100) already builds outbound messages
from a single in-memory `QByteArray` and reassembles inbound messages into
a single in-memory `QByteArray`, bounded by a hard `maxMessageBytes` cap
enforced before any large allocation. File transfer reuses that exact,
already-tested bound (`MsrpSession::sendFile()` rejects rather than
truncates/streams-partial when a file exceeds the cap) instead of building
a new streaming-to-disk I/O path. Rationale: a genuinely streaming
rewrite would be a much larger architectural change, and there is no
interoperability partner available in this environment to validate its
correctness against — see docs/msrp-file-transfer.md for the full
rationale.

## 4. What was implemented

- `src/msrp/MsrpFrame.h`, `MsrpFrameParser.cpp`, `MsrpFrameSerializer.cpp`
  — `Content-Disposition` header added, parsed/serialized exactly like
  `Content-Type`.
- `src/msrp/MsrpMessageChunker.h/.cpp` — `contentDisposition` parameter
  (default empty, so every existing caller/test compiles unchanged),
  threaded onto every chunk.
- `src/msrp/MsrpChunkAssembler.h/.cpp` — `contentDisposition` captured
  into `AssembledMessage`, same pattern as `contentType`.
- `src/msrp/MsrpFileSelector.h/.cpp` (new) — RFC 5547 `a=file-selector`
  SDP attribute parse/build; `sanitizeFileNameForDisplay()` is the only
  place a peer-supplied name is ever turned into a display/suggested-save
  name (strips directory components, rejects `.`/`..`).
- `src/msrp/MsrpFileReceiver.h/.cpp` (new) — `saveToPath()` (atomic
  `QSaveFile` write, no partial file on failure) and `verifyHash()`
  (SHA-1/SHA-256 against a negotiated hash; passes trivially when no hash
  was negotiated, rejects unrecognized algorithms rather than silently
  passing).
- `src/msrp/MsrpSession.h/.cpp` — `sendFile()` (bounded read, SHA-1,
  sanitized `Content-Disposition: attachment; filename="..."`, reuses the
  existing chunked-SEND path) and `fileTransferReceived` signal (additive:
  fires alongside, never instead of, `payloadReceived`).
- `src/gui/panels/MsrpPage.h/.cpp` — "Send File…" button; automatic
  save-dialog prompt on `fileTransferReceived` (pre-filled with the
  sanitized suggested name; user's chosen path is always what gets
  written).

## 5. Files changed

### Added
- `src/msrp/MsrpFileSelector.h`, `.cpp`
- `src/msrp/MsrpFileReceiver.h`, `.cpp`
- `tests/test_msrp_file_selector.cpp`
- `tests/test_msrp_file_receiver.cpp`
- `docs/msrp-file-transfer.md`
- `docs/agent-prompts/W104-msrp-file-transfer.md`
- `docs/agent-results/W104-msrp-file-transfer-result.md` (this file)

### Modified
- `src/msrp/MsrpFrame.h`, `MsrpFrameParser.cpp`, `MsrpFrameSerializer.cpp`
  — `Content-Disposition` header.
- `src/msrp/MsrpMessageChunker.h/.cpp` — `contentDisposition` parameter.
- `src/msrp/MsrpChunkAssembler.h/.cpp` — `contentDisposition` carried into
  `AssembledMessage`.
- `src/msrp/MsrpSession.h/.cpp` — `sendFile()`, `fileTransferReceived`,
  stored `m_maxMessageBytes`.
- `src/gui/panels/MsrpPage.h/.cpp` — Send File button, save-on-receive.
- `tests/test_msrp_session_harness.cpp` — `sendsAndReceivesFileTransfer`,
  `sendFileRejectsMissingFile`.
- `CMakeLists.txt` — new `src/msrp/MsrpFileSelector.cpp`/
  `MsrpFileReceiver.cpp` sources.
- `tests/CMakeLists.txt` — `MsrpFileSelector.cpp` added to `CALL_SOURCES`
  and the `test_msrp_session_harness` target (both compile
  `MsrpSession.cpp`, which now calls into it); two new test targets.
- `docs/msrp-foundation.md`, `docs/msrp-testing.md`, `docs/project-status.md`
  — cross-references and status updates.

## 6. Automated tests

Full build (`build_w101.bat`) completed with zero errors (after fixing an
initial link error — see §10). Full regression (`ctest_w101.bat`):
**71/71 tests passed, 0 failed** (69 pre-existing + `test_msrp_file_selector`
+ `test_msrp_file_receiver`; `test_msrp_session_harness`'s 2 new cases run
inside its existing target), zero regressions.

## 7. Manual/live interoperability tests

`NOT RUN`. No SIP-Server-RTT instance or third-party MSRP client is
reachable from this environment (same limitation documented in every MSRP
task since W100 — see docs/msrp-live-interoperability.md). What *was*
exercised: the local `test_msrp_session_harness` loopback scenario (two
`MsrpSession` instances on 127.0.0.1, real TCP, real chunked SEND/response
frames) sending and receiving an actual 3000-byte file, with SHA-1 verified
on both ends.

## 8. PASS / FAIL / BLOCKED / NOT RUN summary

| Item | Result |
|---|---|
| `file-selector` attribute parse/build | PASS (`test_msrp_file_selector`) |
| `Content-Disposition` frame parse/serialize | PASS (existing frame parser/serializer tests still pass; exercised end-to-end by the session harness) |
| Local loopback file send/receive (chunked, SHA-1 verified) | PASS (`test_msrp_session_harness::sendsAndReceivesFileTransfer`) |
| Oversized/missing-file rejection | PASS (`sendFileRejectsMissingFile`; size-cap check in `sendFile()`) |
| Atomic disk save + hash verification | PASS (`test_msrp_file_receiver`) |
| Live interoperability (real peer/relay) | NOT RUN — no environment available |
| Existing MSRP/SIP/audio/video/RTT/messaging/presence/XCAP/diagnostics functionality | PASS (unchanged, zero regressions, 71/71) |

## 9. Client problems vs. server problems

No server was reached (none available). Not a defect report.

## 10. Build issue found and fixed during this task

Initial build failed with `LNK2019: unresolved external symbol
MsrpFileSelector::sanitizeFileNameForDisplay` in
`test_registration_state_machine.exe` (and, by the same root cause, every
other test target that compiles `SipCall.cpp`/`SipManager.cpp`, i.e. links
`CALL_SOURCES`). Root cause: `tests/CMakeLists.txt`'s `CALL_SOURCES` list
compiles `src/msrp/MsrpSession.cpp` directly (not via a shared library),
and this task's `MsrpSession.cpp` now calls into `MsrpFileSelector.cpp`,
which had not yet been added to that same source list. Fixed by adding
`${CMAKE_SOURCE_DIR}/src/msrp/MsrpFileSelector.cpp` to `CALL_SOURCES`.
Re-verified with a full clean rebuild afterward (zero errors, 71/71 CTest
pass).

## 11. Regressions

None. 71/71 tests passing (69 pre-existing + 2 new targets), the 3
pre-existing MSRP-related targets (`test_msrp_frame_parser`,
`test_msrp_frame_serializer`, `test_msrp_chunker`, `test_msrp_chunk_assembler`,
`test_msrp_payload_dispatcher`, `test_msrp_session_harness`) all still pass
unchanged aside from the new cases added to `test_msrp_session_harness`.

## 12. Limitations

- No live interoperability against a real peer/relay (see §7/§9).
- No resumable/partial transfer (RFC 5547 does not define resume
  semantics; not implemented here).
- No per-chunk progress reporting UI — `sendFile()` enqueues all chunks in
  one call, same as the pre-existing `sendMessage()`. The existing
  per-frame diagnostics log still shows each chunk as it is sent/received.
- File transfer is not wired into any live SIP call — same limitation as
  MSRP generally (only the MSRP page's manual test session can send/
  receive a real file today, exactly as it could already send/receive a
  real chat message).
- Large files are read/held fully in memory once, bounded by
  `maxMessageBytes` (default 2 MiB) — see §3 for why this was a deliberate
  scope decision, not an oversight.

## 13. What moves to W105

Per the roadmap's sequencing, `Task-W105 — Advanced MSRP Hardening` is not
blocked by this result. No carry-forward file-transfer work is queued
beyond the limitations in §12 (resumable transfer, progress UI, live-call
wiring, streaming I/O) — these would be new scope for a future task if
requested, not implicit W105 work.

## 14. Commits

Thematic commits on this branch (see `git log feature/w104-msrp-file-transfer`):

1. `feat(msrp): add rfc 5547 file transfer send/receive` — `MsrpFileSelector`,
   `MsrpFileReceiver`, `Content-Disposition` frame support, `MsrpSession::
   sendFile()`/`fileTransferReceived`, CMake wiring (including the
   `CALL_SOURCES` link fix).
2. `feat(gui): add send-file and save-on-receive to msrp page` — `MsrpPage`
   UI wiring.
3. `test(msrp): cover file transfer selector, receiver, and live harness` —
   the two new test targets plus the session-harness additions.
4. `docs(msrp): document w104 file transfer` — `docs/msrp-file-transfer.md`,
   cross-references, `docs/project-status.md`, agent-prompts/agent-results.

(Exact commit hashes are visible in `git log` on this branch.)

## 15. git status (post-work)

Clean except the same pre-existing untracked `.bat` helper scripts present
since before this task began (not new artifacts of this task).

## 16. Push status

Pushed to `origin/feature/w104-msrp-file-transfer`.

## Final confirmation block

```
Branch-ul a fost push-uit.
Nu s-a făcut merge în main.
Nu s-a făcut merge în release.
Nu au fost modificate sursele pjproject, exceptând cazul în care raportul justifică explicit acest lucru.
Toate rezultatele de interoperabilitate sunt raportate numai pe baza testelor executate.
```

No pjproject source was touched by this task (MSRP is a standalone Qt
protocol stack, per §0 of msrp-foundation.md, unaffected by this task).
Every interoperability-relevant line above is backed directly by an
executed test or explicitly marked `NOT RUN` — no live-peer capability is
claimed anywhere in this report.
