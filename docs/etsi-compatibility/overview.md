# ETSI Compatibility — Overview

**Status:** NOT STARTED

## Scope

This section documents the optional ETSI emergency-calling compatibility modules. These are not required for standard SIP operation. They are compiled only when `BUILD_ETSI=ON` is passed to CMake.

## Standards Coverage

| Standard | Description | Status |
|---|---|---|
| ETSI TS 103 479 | Emergency call handling (PSAP interconnect) | NOT STARTED |
| ETSI TS 103 480 | Emergency call test procedures | NOT STARTED |
| ETSI TS 103 698 | LMPE — Location and Metadata per Event | NOT STARTED |

## Module Architecture

```
src/etsi/
  Ts103479/        — Emergency call handling
  Ts103480/        — Test/conformance support
  Ts103698Lmpe/    — LMPE messaging
```

Each module:
- Has its own CMake target
- Is independently testable
- Does not affect the base SIP client build
- Is documented in its own mapping file under `docs/etsi-compatibility/`

## Status Labels

| Label | Meaning |
|---|---|
| NOT STARTED | No implementation exists |
| PARTIAL | Skeleton or partial implementation |
| IMPLEMENTED | Feature is coded |
| TESTED | Tested against standard or test tool |
| GAP | Known gap between implementation and standard |

## Important Disclaimer

ETSI compliance is not claimed unless a module is marked TESTED against the relevant conformance test suite. IMPLEMENTED means the code exists and functions; it does not mean it meets all normative requirements of the standard.
