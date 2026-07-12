# LMPE — Location and Metadata per Event

**Status:** BLOCKED (codec implementation) — see Task W103 audit below.

## Task W103 audit (2026-07-12)

Before writing any LMPE codec code, Task W103 required confirming the real
wire format against the `SIP-Server-RTT` repository or another
authoritative source. That repository:

- is not configured as a git remote anywhere in this repo or its sibling
  repos under `f:\Project\Iryme`;
- does not exist as a local sibling directory;
- returns HTTP 404 at `https://github.com/Iryme/SIP-Server-RTT`.

No other authoritative source for the LMPE wire format (content-type,
framing, version, required fields) was found in this workspace. The
`lmpe/` module in the sibling `PJSIP-windows-app-emergency` repository
(`LmpeMessage.h`, `LmpeSession.{h,cpp}`, `LmpeProtocolAdapter.{h,cpp}`)
defines only a UI-facing message struct — its `.cpp` files are empty and
none of it encodes/decodes an actual wire format.

Per the task's own rule, the codec is **not implemented** and the format is
**not invented**. Only a neutral interface (`src/etsi/LmpeCodec.h`) and a
fail-closed placeholder (`src/etsi/UnconfirmedLmpeCodec.h`, guarded by
`tests/test_lmpe_codec_unconfirmed.cpp`) were added, so a future task has a
seam to implement against once a real source is available — see
`docs/agent-results/W103-lmpe-foundation-result.md`.

## Overview

LMPE (as defined in ETSI TS 103 698) is a structured messaging format for conveying location and metadata alongside emergency or standard calls. The LMPE panel in the UI is a placeholder for this future module.

## UI

The LMPE tab in the right panel contains:
- Session state label
- Message list
- Input field + Send button
- Delivery/status placeholder per message

## Architecture (Planned)

```
LmpeSession
  ├── LmpeEncoder   — serializes LMPE XML/JSON message
  ├── LmpeDecoder   — parses incoming LMPE data
  └── LmpeTransport — SIP MESSAGE or multipart body transport
```

## Standards Reference

- ETSI TS 103 698 — LMPE specification
- SIP MESSAGE method for session-independent transport
- Multipart MIME body for in-dialog transport

## Implementation Notes

- LMPE is an optional ETSI module.
- The base SIP client provides the UI panel placeholder only.
- Full implementation requires `BUILD_ETSI=ON` CMake option.
- LMPE messages are logged at `INFO` level (content) and `DEBUG` level (protocol detail).
- Message content must never contain authentication credentials.
