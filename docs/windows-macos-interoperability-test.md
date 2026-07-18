# Windows ↔ macOS Interoperability Test

Run this checklist with two separate physical or virtual machines, a SIP test
server, two disposable accounts, and retained client/server diagnostics.

| Direction | Scenario | Evidence required |
|---|---|---|
| Windows → macOS | REGISTER and audio call | 200 OK, two-way audio, hangup |
| macOS → Windows | REGISTER and audio call | 200 OK, two-way audio, hangup |
| Both | Video add/answer | negotiated codec, two-way picture, camera LED |
| Both | RFC 4103 RTT | character-by-character TX/RX with audio active |
| Both | SIP MESSAGE | plain text only in Client chat; no CPIM/XML bubble |
| Both | IMDN/is-composing | visible only in Tools diagnostics, not chat |
| Both | MSRP/file transfer if enabled | negotiated path, content integrity |
| Both | LMPE | remains unavailable and emits no LMPE SDP/payload |
| macOS | permissions | first-use prompts; deny/re-enable behavior |
| macOS | Keychain | save/load/delete disposable credential |

Also verify the macOS ZIP on a second clean Apple Silicon Mac and the Windows
portable ZIP on a clean Windows 10/11 machine. Record app versions, artifact
hashes, OS versions, SIP server, codecs, account directions, logs, screenshots,
and each result as PASS/FAIL/BLOCKED/NOT RUN.

W113G status: **NOT RUN / BLOCKED**. This session had one macOS ARM64 host, no
Windows machine, no second clean Mac, no SIP credentials/server, and no
reference peer. No cross-platform result is inferred from unit tests.

W113H status: **NOT RUN / BLOCKED** (inverse of W113G). This session had one
Windows 11 host at version 1.6.8 (Debug+Release build, 81/81 CTest, portable
bundle generated and verified) but no macOS device, no SIP server/accounts,
and no reference peer — the mirror image of W113G's gap. Combined, W113G and
W113H have now separately validated that each platform builds, tests, and
packages cleanly at 1.6.8 in isolation; neither session has been able to join
both platforms to a live server at the same time. See
[agent-results/W113H-windows-macos-live-interoperability-result.md](agent-results/W113H-windows-macos-live-interoperability-result.md)
for the full matrix and the one Windows-specific finding (`PJ_HAS_SSL_SOCK 0`
— this Windows PJSIP build has no TLS transport compiled in at all, so a
future live TLS scenario needs a PJSIP rebuild with OpenSSL located first).

