# W110 Security Review — SIP-Client-Audio-Video-RTT

Scope: credential storage/lifetime, logging/export redaction, TLS/cert
handling, MSRP connection hijacking, relay auth, parser DoS, file transfer,
path validation, export privacy. All items below were verified by reading
the actual code (see [W110-findings.md](W110-findings.md) for file/line
references on each numbered finding).

## Threat model (short form)

| Asset | Attacker | Entry point | Impact | Mitigation | Status |
|---|---|---|---|---|---|
| SIP/MSRP/XCAP credentials | Local user reading logs/exports, or a network peer | Diagnostics export, raw SIP/MSRP trace, XCAP request logging | Credential disclosure | `SipTraceLogger::redactCredentials` strips `Authorization`/`Proxy-Authorization` before storage; `XcapUrlRedactor` strips userinfo; MSRP relay diagnostics structurally cannot hold nonce/response fields | **Verified sound** |
| Stored account passwords | Local user/malware reading config files | `AppSettings`/`SipProfileManager` ini files | Credential disclosure at rest | Passwords never written to plaintext ini; delegated entirely to `CredentialStore` (Windows Credential Manager backend) | **Verified sound** |
| MSRP direct session | Network peer racing the real remote party for the listening port | Inbound TCP connection to the negotiated MSRP port | Session hijack, message interception | To-Path/session-id validated on every inbound SEND/REPORT (`MsrpSession::toPathTargetsThisSession`); rejected connections get `relisten()` instead of a permanent deny (fixed under W105/W106, reconfirmed here) | **Verified sound** |
| MSRP relay allocation | Malicious/misbehaving relay or MITM on the control connection | RFC 4976 AUTH/allocation exchange | Credential/nonce leak, retry-loop DoS | Digest material never logged; retries bounded by `maxRetries`; a 401/407 while already `WaitingAllocation` is a hard failure, not a loop | **Verified sound** |
| XCAP documents | Malicious XCAP server or a crafted `xui`/document name | XCAP request URI construction | Path traversal (e.g. `xui="../otheruser"`) | `XcapModels::buildUri`/`encodeSegment` percent-encode all path segments (fixed under W104, reconfirmed applied uniformly here) | **Verified sound** |
| XML-bearing bodies (PIDF/IMDN/is-composing) | Network peer sending a crafted body | `QXmlStreamReader` parsing | XXE / entity-expansion DoS | `QXmlStreamReader` has no DTD/entity support by default (confirmed, no entity resolver configured anywhere); `XcapXmlValidator` additionally rejects any `<!DOCTYPE` outright as defense-in-depth | **Verified sound** (see W110-F015 for a minor input-size-cap inconsistency, not itself exploitable) |
| Compressed diagnostic payloads | Peer sending a crafted deflate/gzip stream | `DeflateDecoder` | Decompression bomb | Explicit `maxCompressedInput`/`maxDecompressedOutput` (ratio-capped) checked before every append; malformed streams fail cleanly, no infinite loop found | **Verified sound** |
| MSRP frames / chunked bodies | Peer sending malformed/oversized frames | `MsrpFrameParser`, `MsrpChunkAssembler` | Buffer overflow, unbounded allocation, resync failure | Byte-Range/total-size validated before any `resize()`; frame parser guarantees forward progress on corruption (resync-or-clear); body kept as `QByteArray` end-to-end (no embedded-NUL truncation) | **Verified sound** |
| Received files | Peer sending a crafted filename or tampered body | `MsrpFileSelector`/`MsrpFileReceiver`, save dialog | Path traversal, overwrite, silent tampering | Path traversal prevented (save always goes through `QFileDialog`, suggested name only ever a basename); **negotiated hash is never actually verified against received bytes** | **Gap — W110-F003 (High)** |
| Received filenames | Peer suggesting `CON`/`PRN`/etc. as a filename | Save-dialog default filename | Confusing save failure (not data loss) | Path-traversal characters stripped; reserved device names not special-cased | **Gap — W110-F016 (Low)** |

## Notable non-findings worth recording

- **No hardcoded credentials, IPs, domains, or SIP URIs** were found
  anywhere in `src/` outside of documented, config-sourced defaults and
  explicit placeholder/display strings (`"sip:unknown@unknown"` shown only
  when `remoteUri` is empty).
- **CRLF/NUL header injection** is guarded in both directions: custom SIP
  headers reject embedded CR/LF (fixed under W104), and
  `MsrpFrameSerializer::serialize` rejects any header value containing
  CR/LF/NUL before writing.
- **Content-Length correctness**: every `SipMessageComposer::compose*`
  function computes `Content-Length` from `QByteArray::size()` (UTF-8
  bytes), never `QString::length()` — no multi-byte undercount bug that
  could desync a body boundary.
- **No command injection surface** was found — no code in `src/` shells
  out to an external process with peer-controlled input.

## Items requiring follow-up (see roadmap)

1. **W110-F003** (High) — file-transfer hash negotiated but never verified;
   silent tampering/corruption acceptance. → W114.
2. **W110-F016** (Low) — Windows reserved device names not screened in
   suggested filenames. → W114.
3. **W110-F015** (Low) — `ImdnParser`/`IsComposingParser` lack the same
   input-size cap `PidfParser` already enforces (defense-in-depth only,
   not independently exploitable given current callers' own upstream
   caps). → W114.

No Critical-severity security issue was found in this review.
