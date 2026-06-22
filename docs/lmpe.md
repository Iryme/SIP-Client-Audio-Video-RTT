# LMPE — Location and Metadata per Event

**Status:** NOT STARTED

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
