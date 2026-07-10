# Content-Encoding Diagnostics (Task W095)

Cross-reference: [messaging-diagnostics.md](messaging-diagnostics.md) (the
overall Messaging Diagnostics feature this hardens),
[windows-trace-json-export.md](windows-trace-json-export.md) (the interop
JSON export schema this extends to v2), and
[rcs-ft-http-diagnostics.md](rcs-ft-http-diagnostics.md) (the companion RCS
FT HTTP parser added in the same task).

## Why this exists

Real interop testing against Linphone iOS showed:

- some `message/imdn+xml` bodies arrive with `Content-Encoding: deflate`
  and decode correctly;
- others produced `"deflate decode failed"` or a generic
  `"Content-Type declared message/imdn+xml but no IMDN disposition could be
  parsed."` warning, with no way to tell *why* decoding failed;
- raw SIP bodies are not guaranteed to be UTF-8 text — a compressed body is
  arbitrary binary data, and must never be treated as text before it is
  decompressed.

Task W092 added a first, narrower `Content-Encoding: deflate` decoder (zlib
format only, via `qUncompress()`). Task W095 replaces it with a hardened
pipeline: binary-safe body extraction, an in-house zlib/raw-deflate/gzip
decoder with decompression-bomb limits, and structured decode metadata
surfaced through the whole diagnostics pipeline (trace entry → event →
JSON export → UI).

## Pipeline: raw bytes → decoded bytes → text body → body preview

This is the core invariant the whole feature is built around — each stage
is a distinct, explicit step, and a compressed/binary body is **never**
converted to `QString` before it has been decompressed:

1. **Raw body bytes** (`SipBodyExtractor::extract()`,
   `src/sip/SipBodyExtractor.h/.cpp`) — recovers the exact wire bytes of the
   body. `SipMessageTrace::rawSip` is a `QString` built via
   `QString::fromLatin1()` over the original wire bytes (see
   `PjsipTraceModule.cpp`) — Latin-1 maps every byte 0-255 to one `QChar`
   losslessly, so `rawSip.toLatin1()` recovers those bytes exactly,
   including embedded NUL bytes. `SipBodyExtractor` locates the
   header/body separator and the `Content-Length` header purely as byte
   sequences (never via `QString::split`/`trimmed`, which are text
   operations) and returns exactly `Content-Length` bytes of body — not
   "everything after the blank line" unless `Content-Length` is absent or
   unusable, in which case it falls back to that (matching the pre-W095
   behavior for messages without an explicit `Content-Length`, which is how
   every existing unit test fixture is built).
2. **Decoded body bytes** (`DeflateDecoder::decode()`,
   `src/sip/DeflateDecoder.h/.cpp`) — only invoked when
   `Content-Encoding: deflate` is present. Returns a structured `Result`
   (see below) instead of a bare `QByteArray`, so the caller always knows
   *what* decoded (which variant) or *why* it didn't (which error).
3. **Text body** (`MessagingDiagnosticsStore::buildEntry()`) — only the
   decoded (or, for an unencoded body, the original) bytes are ever handed
   to `QString::fromUtf8()`. If decoding failed/was unsupported/hit a
   safety limit, the "effective" byte buffer is left empty — nothing
   downstream (CPIM/IMDN/is-composing/RCS-FT-HTTP parsers) ever attempts to
   XML-parse compressed or binary bytes.
4. **Body preview** (`MessagingTraceEntry::bodyPreview` /
   `decodedBodyPreview`) — a truncated, whitespace-collapsed view of the
   text body from step 3. Never built from raw compressed bytes: a failed
   decode yields an empty preview, not a dump of undecodable binary data.

## Deflate decoder (`DeflateDecoder`)

No third-party zlib is linked into this project (confirmed by grep — there
is no `find_package(ZLIB)` anywhere in `CMakeLists.txt`, and Qt's own
bundled zlib is not exposed for application linkage on Windows). Rather than
add a new dependency, `DeflateDecoder` implements a small, self-contained
RFC 1951 (raw DEFLATE) inflate routine — structured after the well-known
public-domain "puff.c" reference algorithm (Mark Adler / zlib project),
rewritten in C++ against `QByteArray` — and uses it as the single
decompression primitive for all three supported wire formats:

| Variant | Wire format | Detection |
|---|---|---|
| `zlib` | RFC 1950: 2-byte header + raw DEFLATE (+ 4-byte Adler-32, not verified) | 2-byte header passes the RFC 1950 check-bits test (`(CMF<<8\|FLG) % 31 == 0` and `CMF & 0x0F == 8`) |
| `raw-deflate` | RFC 1951, headerless | fallback when the zlib-header check fails, or the zlib-format inflate itself fails |
| `gzip` | RFC 1952: 10+ byte header (+ optional FEXTRA/FNAME/FCOMMENT/FHCRC) + raw DEFLATE + 8-byte CRC32/ISIZE trailer, not verified | **only** attempted when the wire bytes start with the explicit `1F 8B` signature — never guessed otherwise |

Strategy, matching the task's recommendation:

1. If the bytes carry a gzip signature, decode as gzip (parses the variable
   gzip header to find where the DEFLATE data starts, then reuses the same
   inflate routine).
2. Otherwise, if the first two bytes pass the RFC 1950 zlib header check,
   try the zlib-wrapped form.
3. If that header check failed, or the zlib-format inflate itself failed
   (a raw deflate stream can coincidentally pass the 5-bit-wide zlib header
   check), fall back to raw (headerless) DEFLATE.
4. If nothing decodes, return a structured failure — never silently return
   empty data.

Every call returns a `DeflateDecoder::Result`:

```cpp
struct Result {
    bool       ok;
    QByteArray data;
    Variant    variant;       // None/Zlib/RawDeflate/Gzip
    ErrorCode  errorCode;     // None/EmptyInput/InputTooLarge/OutputTooLarge/
                               // RatioExceeded/Corrupt/Unsupported
    QString    errorMessage;
    qint64     inputSize;
    qint64     outputSize;
};
```

**The Adler-32 (zlib) and CRC32 (gzip) trailers are not verified.** This is
a diagnostic decoder, not a correctness-critical transport — skipping the
checksum keeps the implementation self-contained (no extra CRC32/Adler32
code paths) and does not affect whether the payload decodes correctly,
since the checksum only covers integrity of already-decoded bytes.

### Safety limits (decompression-bomb protection)

```cpp
struct Limits {
    qint64 maxCompressedInput{4 * 1024 * 1024};      // 4 MiB, default
    qint64 maxDecompressedOutput{16 * 1024 * 1024};  // 16 MiB, default
    qint64 maxExpansionRatio{1024};                  // decoded <= ratio * compressed
};
```

- **`maxCompressedInput`** is checked before any decompression work starts.
- **`maxDecompressedOutput`** and **`maxExpansionRatio`** are combined into
  a single effective output cap (`min(maxDecompressedOutput, compressedSize
  * maxExpansionRatio)`) that is enforced **during** decompression — every
  literal byte written and every LZ77 back-reference copy checks the running
  output size against this cap and aborts immediately if it would be
  exceeded, before any further (potentially unbounded) allocation. This is
  the standard mitigation against a "decompression bomb": a tiny compressed
  input crafted to expand to gigabytes. Because the check happens inline in
  the inflate loop (not "decompress fully, then check the result size"),
  a bomb is stopped as soon as it starts growing past the limit, not after
  it has already exhausted memory.
- When either limit trips, the result reports `ErrorCode::OutputTooLarge`
  (absolute cap was binding) or `ErrorCode::RatioExceeded` (the ratio-based
  cap was the tighter one) — `MessagingDiagnosticsStore` maps both to
  `decodeStatus = limit-exceeded`.

## Decode metadata (`MessagingTraceEntry` / `MessagingEvent` / export)

| Field | Meaning |
|---|---|
| `contentEncoding` | The raw `Content-Encoding` header value, verbatim; empty when absent |
| `decodeStatus` | `not-needed` (no `Content-Encoding`) / `decoded` / `failed` / `unsupported` (a `Content-Encoding` value other than `deflate`) / `limit-exceeded` |
| `decodeVariant` | `zlib` / `raw-deflate` / `gzip` / `none` |
| `compressedBodyLength` | Size of the raw body bytes handed to the decoder |
| `decodedBodyLength` | Size of the decoded output, when `decodeStatus == decoded` |
| `decodeError` | Human-readable failure reason; only present when relevant |
| `decodedBodyPreview` | Truncated preview of the decoded text; empty when decoding failed |

These fields exist on `MessagingTraceEntry` (built once, in
`MessagingDiagnosticsStore::buildEntry()`), are copied onto `MessagingEvent`
by `MessagingEventStore::mapFromTraceEntry()`, and are exported by both
`MessagingEventStore::exportToJson()`/`exportToText()` and
`InteropTraceExporter::exportToJson()` (schema v2 — see
[windows-trace-json-export.md](windows-trace-json-export.md)).

**The raw compressed bytes are never exposed in any preview field.** A
failed/unsupported/limit-exceeded decode leaves `bodyPreview` and
`decodedBodyPreview` empty rather than dumping binary garbage as text.

`rawSipRedacted` / `rawSip` (the full raw SIP message text, with
`Authorization`/`Proxy-Authorization` header values already redacted
upstream by `SipTraceLogger`) is **unchanged** by this feature — it still
carries the original wire bytes (Latin-1 round-tripped), compressed body
included, exactly as before.

## IMDN retry after decode

`message/imdn+xml` is parsed by `ImdnParser::parse()` exactly as before
(Task W090/W091) — the only change is *what body it is ever handed*:

- No `Content-Encoding` → the original (never compressed) body, as always.
- `Content-Encoding: deflate`, decoded successfully → the **decoded** body.
  `message-id`, `delivered`/`displayed`/`failed`/`error` disposition, and
  `original-recipient`/`final-recipient` are all parsed from it normally.
- `Content-Encoding: deflate`, decode failed/unsupported/limit-exceeded →
  `ImdnParser::parse()` is never called; `MessagingTraceEntry::imdn.present`
  stays `false`.

`MessagingEventStore::mapFromTraceEntry()` gives the failed-decode case a
precise warning instead of the generic "no disposition could be parsed"
message:

```
decodeStatus == Failed/LimitExceeded  -> parseStatus = Partial, warning: "deflate decode failed"
decodeStatus == Unsupported           -> parseStatus = Partial, warning: "Content-Encoding \"<value>\" is not supported — body left undecoded."
```

The generic per-content-kind warnings (e.g. "Content-Type declared
message/imdn+xml but no IMDN disposition could be parsed.") are now
suppressed when a decode-specific warning already explains *why* nothing
could be parsed, so a compressed-payload failure surfaces exactly one clear
reason instead of two overlapping ones.

## What is diagnostic-only / what is NOT implemented

- Only `Content-Encoding: deflate` is ever attempted for decompression;
  any other value (e.g. `identity`, `br`, `compress`, or an unrecognized
  token) is reported as `decodeStatus = unsupported` and the body is left
  undecoded — it is never guessed at or force-decoded.
- Gzip is decoded opportunistically only when the wire bytes carry the
  explicit `1F 8B` signature (per the task's "eventual gzip doar dacă
  header-ul sau semnătura indică explicit gzip" instruction) — it is not
  advertised as a supported `Content-Encoding` value on its own.
- Adler-32/CRC32 trailer verification is not implemented (see above).
- Decoding is applied once at the outer SIP body level, before CPIM
  detection. A hypothetical UA that compresses only an *inner*
  CPIM-wrapped body while leaving the outer `message/cpim` body
  uncompressed is not supported (not observed in practice, not in scope).
- No file is downloaded, no MSRP session is opened, and no network access
  of any kind happens anywhere in this pipeline — decoding operates purely
  on bytes already captured from the local SIP trace.
