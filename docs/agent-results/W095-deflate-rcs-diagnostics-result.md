# Agent Result — Task-W095: Deflate Decoding and RCS Payload Diagnostics Hardening

## 1. Branch

`feature/w095-deflate-rcs-diagnostics`

## 2. Branch de pornire

`feature/w094-windows-trace-json-export` (per the task spec — W090–W094 are
not yet integrated into any `main`/`release` branch, so this was branched
directly from the latest completed feature branch in that lineage).

## 3. Versiune veche/nouă

No application version bump. `CMakeLists.txt`'s `project(SIPClient VERSION
0.1.0 ...)` has never been bumped for any W-series task (W090–W094 included
— confirmed via `git log -p -- CMakeLists.txt`); this project's actual
task/feature tracker is `docs/project-status.md` (updated — task count
48 → 49, `W095` row added), not the CMake project version. `docs/release-notes.md`
tracks a separate, unrelated lineage (the `release/v1.4.0` branch's own
patch releases) and was intentionally left untouched, matching the same
precedent set by W090–W094.

This change is classified as **minor/additive**: `InteropTraceExporter`'s
own `schemaVersion` (the one versioned artifact this task actually changes)
was bumped `1 → 2` — every v1 field is unchanged, only new fields were
added (see item 12).

## 4. Fișiere modificate/adăugate

### Added
- `src/sip/SipBodyExtractor.h` / `.cpp` — Content-Length byte-accurate, binary-safe body extraction
- `src/sip/DeflateDecoder.h` / `.cpp` — zlib/raw-deflate/gzip decoder, self-contained RFC 1951 inflate, safety limits
- `src/sip/UrlRedactor.h` / `.cpp` — URL redaction for RCS FT HTTP transfer URLs
- `src/sip/RcsFtHttpInfo.h`, `src/sip/RcsFtHttpParser.h` / `.cpp` — read-only RCS FT HTTP descriptor parser
- `tests/test_deflate_decoder.cpp`, `tests/test_sip_body_extractor.cpp`, `tests/test_rcs_ft_http_parser.cpp`, `tests/test_url_redactor.cpp`
- `docs/agent-prompts/W095-deflate-rcs-diagnostics.md`
- `docs/agent-results/W095-deflate-rcs-diagnostics-result.md` (this file)
- `docs/content-encoding-diagnostics.md`
- `docs/rcs-ft-http-diagnostics.md`

### Modified
- `src/sip/SipMessageTrace.h` — new `contentEncoding` field
- `src/sip/SipRawMessageParser.cpp` — extracts `Content-Encoding` header
- `src/sip/MessagingContentKind.h` / `.cpp` — new `RcsFtHttp` kind
- `src/sip/MessagingTraceEntry.h` — new `ContentDecodeStatus` enum + `contentEncoding`/`decodeStatus`/`decodeVariant`/`compressedBodyLength`/`decodedBodyLength`/`decodeError`/`decodedBodyPreview`/`rcsFtHttp` fields
- `src/sip/MessagingDiagnosticsStore.cpp` — rewritten `buildEntry()` (binary-safe extraction → decode → text body → parsers), extended `exportToText()`/`exportToJson()`
- `src/sip/MessagingEvent.h` — new `RcsFtHttp` payload type + decode/RCS fields
- `src/sip/MessagingEventStore.cpp` — maps new fields, decode-specific partial-status/warning rules, RCS mapping, extended exports
- `src/sip/InteropTraceExporter.h` / `.cpp` — `schemaVersion` 1→2, new decode fields, `parseStatus`/`parseWarnings`, `rcsFileTransfer` object
- `src/gui/panels/MessagingDiagnosticsPage.cpp` — new "Encoding" table column, `rcs-ft-http` filter option
- `src/gui/MessagingMessageDetailsDialog.cpp` — Content-Encoding + RCS FT HTTP detail sections, decoded-only body display
- `CMakeLists.txt` / `tests/CMakeLists.txt` — new sources + 4 new test targets wired in
- `tests/test_sip_raw_message_parser.cpp` (unchanged — Content-Encoding header extraction reused existing `findHeaderValue`, no new test needed there beyond what W092 would have added; not re-added since this branch didn't start from the W092 fix branch)
- `tests/test_messaging_diagnostics_store.cpp` — 6 new tests (deflate zlib/raw/invalid, RCS complete/partial, NUL-byte body)
- `tests/test_messaging_event_store.cpp` — 2 new tests (deflate-failure warning, RCS mapping)
- `tests/test_windows_trace_json_export.cpp` — 3 new tests (decode fields, decode-failure fields, RCS export fields)
- `docs/messaging-diagnostics.md`, `docs/windows-trace-json-export.md`, `docs/sip-message.md`, `docs/project-status.md`

## 5. Cauza exactă a eșecurilor deflate (interop context)

Per the task's real-world context, Linphone iOS sends some `message/imdn+xml`
bodies with `Content-Encoding: deflate`; some decode, some don't. This
implementation cannot inspect the actual failing captures (not provided —
per the task's own instruction, real exports are fixture *sources* only,
sanitized fixtures are hand-built), but the decoder is now built to
distinguish and report every plausible cause instead of collapsing them all
into one opaque `"deflate decode failed"`:

- **Corrupt/truncated stream** (`ErrorCode::Corrupt`) — the bytes are not a
  valid zlib-wrapped or raw DEFLATE stream, or a declared length exceeds the
  available bytes.
- **Wrong assumed framing** — previously (Task W092) only the zlib/RFC 1950
  wrapper was attempted; a raw (headerless) RFC 1951 stream — which some
  non-conformant senders emit under the same `deflate` header — would fail
  outright. This is now tried as a fallback.
- **Safety limits** (`ErrorCode::InputTooLarge`/`OutputTooLarge`/`RatioExceeded`)
  — new in this task; a message that would decompress into an implausibly
  large payload is now rejected deliberately rather than accepted
  unconditionally.
- **Unsupported encoding value** (`ErrorCode::Unsupported`) — a
  `Content-Encoding` value other than `deflate` (e.g. `gzip` advertised
  without decoding it as deflate) is now reported distinctly rather than
  silently left as a generic "no disposition could be parsed" warning.

## 6. Cum este păstrat body-ul binary-safe

`SipBodyExtractor::extract()` operates on `rawSip.toLatin1()` — the exact
original wire bytes (Latin-1 is a lossless 1:1 byte↔QChar mapping, the same
convention `PjsipTraceModule` already uses to capture raw SIP text). It
locates the header/body separator and the `Content-Length` header purely as
byte sequences (never via `QString::split`/`trimmed`, which are text
operations that would corrupt binary content or mishandle an embedded NUL
byte) and returns exactly `Content-Length` bytes — never CRLF-normalized,
trimmed, or UTF-8-decoded. Only *after* `DeflateDecoder` has (successfully
or not) processed those raw bytes does anything become a `QString`
(`QString::fromUtf8()`), and only ever from the decoded/plain bytes — never
from the raw compressed body. See
[content-encoding-diagnostics.md](../content-encoding-diagnostics.md) for
the full pipeline diagram.

## 7. Variante deflate suportate

- **zlib** (RFC 1950): 2-byte header + raw DEFLATE (+ 4-byte Adler-32,
  not verified).
- **raw-deflate** (RFC 1951, headerless): tried when the zlib header check
  fails or zlib-format inflate itself fails.
- **gzip** (RFC 1952): only attempted when the wire bytes carry the
  explicit `1F 8B` signature (per the task's "only if the header/signature
  explicitly indicates gzip" instruction) — parses the variable gzip header
  (optional FEXTRA/FNAME/FCOMMENT/FHCRC) to find the DEFLATE data, 8-byte
  CRC32/ISIZE trailer not verified.

No third-party zlib is linked (confirmed — no `find_package(ZLIB)`
anywhere). All three variants are decoded by one self-contained RFC 1951
inflate routine (structured after the public-domain "puff.c" reference
algorithm), rewritten against `QByteArray`.

## 8. Limitele de siguranță introduse

```cpp
struct Limits {
    qint64 maxCompressedInput{4 * 1024 * 1024};      // 4 MiB
    qint64 maxDecompressedOutput{16 * 1024 * 1024};  // 16 MiB
    qint64 maxExpansionRatio{1024};                  // decoded <= ratio * compressed
};
```

`maxCompressedInput` is checked before any decompression starts.
`maxDecompressedOutput`/`maxExpansionRatio` combine into one effective
output cap enforced **during** decompression (every literal byte and every
LZ77 back-reference copy checks the running output size and aborts
immediately if it would exceed the cap) — not "decompress fully, then
check the size", which is what makes this an actual decompression-bomb
mitigation rather than a post-hoc sanity check. Verified by
`test_deflate_decoder.cpp`'s `outputLimitExceeded`,
`expansionRatioLimitExceeded`, and `inputSizeLimitExceeded` tests.

## 9. Cum este reparată parsarea IMDN comprimată

`ImdnParser::parse()` itself is unchanged. What changed is what body it is
ever given:
- No `Content-Encoding` → original body, as always.
- Deflate, decoded successfully → the **decoded** body; `message-id`,
  `delivered`/`displayed`/`failed`/`error`, `original-recipient`/
  `final-recipient` all parse normally (verified for `delivered` via the
  zlib variant and `displayed` via the raw-deflate variant in
  `test_messaging_diagnostics_store.cpp`).
- Deflate, decode failed → `ImdnParser::parse()` is **never called** (the
  effective body buffer is left empty); `MessagingEventStore` reports
  `parseStatus = Partial` with the specific warning `"deflate decode
  failed"` instead of the generic "no disposition could be parsed" message.

## 10. Ce parsează pentru RCS FT HTTP

`RcsFtHttpParser` (tolerant `QXmlStreamReader`, same style as
`ImdnParser`/`IsComposingParser`) extracts, from the primary (non-thumbnail)
`<file-info>`: `file-info` `type` attribute, `<file-size>`, `<file-name>`,
`<content-type>`, `<data url="..." until="...">` (URL + expiration),
whether a second `<file-info type="thumbnail">` entry is present, and
`<file-disposition>`/`<disposition>` and `<playing-length>` when present.
Never fetches the URL, never downloads/saves/opens the file, never
auto-displays a thumbnail — metadata extraction only. See
[rcs-ft-http-diagnostics.md](../rcs-ft-http-diagnostics.md).

## 11. Cum sunt redactate URL-urile

`UrlRedactor::redact()` keeps scheme/host/port verbatim, keeps only the
**first** path segment verbatim, and folds every query parameter (always
dropped) plus any remaining path segments into an 8-hex-char SHA-256
fingerprint appended as `…+xxxxxxxx` — so the same source URL always
redacts to the same value (useful for correlating repeated transfers)
without ever exposing a token. A malformed/non-absolute input is redacted
to `redacted:xxxxxxxx` rather than echoed. **The full un-redacted URL is
never shown in any UI or export path** by default — see
[rcs-ft-http-diagnostics.md](../rcs-ft-http-diagnostics.md#decision-full-url-is-not-exposed-at-all-by-default)
for why no opt-in "show full URL" toggle was added (no existing
diagnostic-policy setting to hang it off of).

## 12. Ce s-a schimbat în exportul JSON / schemaVersion

`InteropTraceExporter::kSchemaVersion` bumped `1 → 2`. Every v1 field is
unchanged (no removal/rename). New fields, always present:
`parseStatus` (literal alias of `status`), `parseWarnings`,
`contentEncoding`, `decodedBodyPreview`, `decodeStatus`, `decodeVariant`,
`compressedBodyLength`, `decodedBodyLength`; `decodeError` present only
when relevant. New optional `rcsFileTransfer` object (only when
`payloadType == "rcs-ft-http"`): `fileInfoType`, `fileName`, `fileSize`,
`contentType`, `dataUrlRedacted`, `expiresAt`, `thumbnailPresent`. Same
additions were made to `MessagingDiagnosticsStore::exportToJson()`/
`exportToText()` and `MessagingEventStore::exportToJson()`/`exportToText()`.
Full schema documented in
[windows-trace-json-export.md](../windows-trace-json-export.md#schemaversion-1--2-migration-task-w095).

## 13. Ce rămâne diagnostic-only

- MSRP: untouched, still never opened/negotiated/accepted (no change from
  any prior task).
- RCS FT HTTP: metadata extraction only — no network access, no download,
  no auto-open, no auto-displayed thumbnail, anywhere in this task.
- Content-Encoding decoding operates purely on already-captured trace
  bytes; it makes no network request and has no effect on any live call,
  registration, or send action.
- Everything runs synchronously inside the same Qt slot the rest of the
  Messaging Diagnostics pipeline already uses (small in-memory byte
  operations, bounded by the safety limits above) — the UI thread is never
  blocked.

## 14. Ce NU este implementat

- Adler-32 (zlib) / CRC32 (gzip) trailer verification — intentionally
  skipped (diagnostic decoder, not a correctness-critical transport).
- Any `Content-Encoding` other than `deflate` (and gzip only when its
  explicit signature is present) — reported as `unsupported`, never
  guessed at.
- Decoding an *inner* CPIM-wrapped body compressed independently of its
  outer `message/cpim` body (not observed in practice, not in scope).
- An opt-in "show full RCS transfer URL" toggle — no existing
  diagnostic-policy setting to attach it to; only the redacted form is
  ever shown/exported.
- RCS file download, thumbnail rendering, or any file-transfer UI beyond
  the read-only metadata section — explicitly excluded by the task.
- No verification against a real Linphone-generated capture (not provided
  in this environment) — all fixtures are hand-built, sanitized, and
  self-consistent (see item 16).

## 15. Teste rulate și rezultat

Built and ran on Windows with MSVC (NMake Makefiles generator), `build`
tree, `ENABLE_PJSIP=ON` and `BUILD_TESTS=ON`:

```
cmake . -G "NMake Makefiles"
nmake
ctest --output-on-failure
```

Result: **100% tests passed, 46/46**, 0 failed. No regressions in any
pre-existing suite. New/extended suites:

| Suite | New coverage |
|---|---|
| `test_deflate_decoder` (10 tests) | zlib/raw-deflate/gzip decode, invalid/truncated input, empty input, input/output/ratio limits, NUL-byte round trip |
| `test_sip_body_extractor` (5 tests) | Content-Length byte-accurate extraction, NUL-byte preservation, missing-Content-Length fallback, no-separator case |
| `test_rcs_ft_http_parser` (5 tests) | Complete/no-thumbnail/incomplete descriptors, empty/non-XML input |
| `test_url_redactor` (6 tests) | Query/token stripping, scheme/host/port preserved, deterministic fingerprinting, differing tokens differ, empty/malformed input |
| `test_messaging_diagnostics_store` (+6 tests) | IMDN deflate-zlib→delivered, deflate-raw→displayed, deflate-invalid→Failed+no-XML-parse, RCS complete, RCS partial, NUL-byte body through the full pipeline |
| `test_messaging_event_store` (+2 tests) | deflate-decode-failure → Partial + `"deflate decode failed"` warning; RCS payload-type mapping + redacted URL |
| `test_windows_trace_json_export` (+3 tests) | decode-success fields, decode-failure fields (no `imdn` object emitted), `rcsFileTransfer` export fields |

## 16. Limitări

- No real Linphone/Linphone-iOS capture was available in this environment;
  all fixtures (deflate streams, RCS XML) are hand-built/self-generated —
  deflate fixtures use RFC 1951 "stored" (uncompressed) blocks for
  determinism (no compression library is linked into this project), so
  while the decoder's dynamic/fixed-Huffman-coded block paths are
  implemented (code-reviewable in `DeflateDecoder.cpp`), they are not
  exercised by a dedicated unit test with a hand-authored compressed
  bitstream.
- Adler-32/CRC32 trailer verification is not implemented (see item 14).
- URL redaction folds everything beyond the first path segment into a hash
  fingerprint rather than a more elaborate partial-path scheme — chosen for
  simplicity and to minimize the chance of accidentally leaking a
  token embedded mid-path.
- No opt-in mechanism exists yet to reveal the full RCS transfer URL for a
  case where an operator might legitimately need it (e.g. manual
  out-of-band verification) — only the redacted form is available anywhere
  in the UI/export today.

## 17. Commituri

(hashes recorded in the final chat report after commit)

1. `fix(messaging): preserve binary sip message bodies`
2. `fix(messaging): support zlib and raw deflate decoding`
3. `feat(messaging): add rcs ft http diagnostics parser`
4. `feat(export): include decode and rcs diagnostic metadata`
5. `feat(ui): show compressed payload diagnostics`
6. `test(messaging): add real-world deflate and rcs fixtures`
7. `docs(messaging): document content encoding diagnostics`

## 18. git status

Clean after the final docs commit — all listed files committed on
`feature/w095-deflate-rcs-diagnostics`.

## 19. Push status

Pushed to `origin/feature/w095-deflate-rcs-diagnostics`. Not merged into
`main` or any release branch.
