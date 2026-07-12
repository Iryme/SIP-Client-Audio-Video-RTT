# ETSI TS 103 698 — LMPE Feature Mapping

**Status:** BLOCKED (Task W103) — real wire format not confirmed. No
`SIP-Server-RTT` repository is reachable from this environment (not a
configured remote, not a local sibling, 404 on GitHub) and no other
authoritative LMPE format source exists in this workspace. See
[../lmpe.md](../lmpe.md) and
[../agent-results/W103-lmpe-foundation-result.md](../agent-results/W103-lmpe-foundation-result.md)
for the full audit.

## Overview

ETSI TS 103 698 defines LMPE (Location and Metadata per Event), a structured format for conveying location, caller identity, and context metadata within or alongside SIP sessions.

## Requirements Mapping

| Requirement | Section | Status | Notes |
|---|---|---|---|
| LMPE message format | — | NOT STARTED | |
| SIP MESSAGE transport | — | NOT STARTED | |
| In-dialog multipart body | — | NOT STARTED | |
| Location encoding (PIDF-LO) | — | NOT STARTED | |
| Caller identity fields | — | NOT STARTED | |
| Message delivery confirmation | — | NOT STARTED | |
| LMPE session state machine | — | NOT STARTED | |

## Implementation Plan

To be documented when implementation begins. Implementation requires `BUILD_ETSI=ON`.

## Dependencies

- RFC 4119 (PIDF-LO location format)
- RFC 3261 (SIP base)
- SIP MESSAGE method
