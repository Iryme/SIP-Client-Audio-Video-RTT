# MSRP Diagnostics (SDP Detection Only)

**Task W090** — Added in branch `feature/w090-msrp-lmpe-diagnostics`.
**Task W094** — [Windows Trace JSON Export](windows-trace-json-export.md)
(branch `feature/w094-windows-trace-json-export`) adds an interop-compatible
JSON export that includes these same `SdpMsrpInfo` fields (nested under an
`"msrp"` key) alongside SIP MESSAGE/CPIM/IMDN/is-composing events, for
diffing against a SIP-Server-RTT server-side trace. It reads the fields
described below as-is — no new MSRP parsing or detection logic was added.

## Overview

This document covers the MSRP-specific part of the [Messaging Diagnostics](messaging-diagnostics.md)
feature: detecting MSRP-related attributes in SDP bodies. **This is
detection/diagnostics only.** No MSRP socket is ever opened, no MSRP session
is ever established, and no MSRP session is started implicitly by any code
path added in this task. MSRP is off by default and stays off — there is no
config flag that turns on a real MSRP transport in this task; the "flag" that
exists is simply whether the Messaging Diagnostics page is navigated to,
which only affects what is *displayed*, not what is *sent*.

## What is detected

`SdpMsrpDiagnosticsParser::parse(const QString &sdpText)`
(`src/sip/SdpMsrpDiagnosticsParser.h/.cpp`) scans an SDP body for the first
`m=message` media line and the attribute lines that belong to that media
block (up to the next `m=` line or end of body):

| SDP line | Extracted into `SdpMsrpInfo` field |
|---|---|
| `m=message <port> <proto> <format>` | `mediaLine` (raw line), `transportProtocol` (the `<proto>` token, e.g. `TCP/MSRP` or `TCP/TLS/MSRP`) |
| `a=path:<msrp-uri>[ <msrp-uri> ...]` | `path` |
| `a=accept-types:<types>` | `acceptTypes` |
| `a=setup:<active|passive|actpass>` | `setup` |
| `a=connection:<new|existing>` | `connection` |

### Session-ID extraction

MSRP URIs look like `msrp://host:port/session-id;tcp` (or `msrps://...` for
TLS). `sessionId` is extracted from the **last** URI in `a=path` (the
closest hop to this endpoint, matching how relay chains are read) using the
pattern `/([^/;\s]+);` — the path segment immediately before the `;tcp`
transport suffix.

## Where this runs

`MessagingDiagnosticsStore::buildEntry()` calls
`SdpMsrpDiagnosticsParser::parse()` on the body of **any** captured SIP
message whose body contains an `m=message` line — this includes MESSAGE
bodies as well as INVITE/re-INVITE bodies that offer MSRP alongside (or
instead of) audio/video/RTT. The result is stored in
`MessagingTraceEntry::sdpMsrp` and shown in the Messaging Diagnostics page
and in the SIP Ladder (as an `[MSRP-SDP]` badge).

## Read-only guarantee

- `SdpMsrpDiagnosticsParser` is a pure `QString → SdpMsrpInfo` function: no
  sockets, no PJSIP types, no side effects.
- `MessagingDiagnosticsStore` only ever reads traces already captured by the
  existing `SipTraceLogger` / `PjsipTraceModule` pipeline (used by the SIP
  Ladder) — it does not register any new PJSIP callback and cannot influence
  what SDP is offered or accepted.
- Nothing in this task path calls into `SipCall`, `SipAccount`, or `SipManager`
  to negotiate or start an MSRP media stream.

## Tests (`tests/test_sdp_msrp_diagnostics_parser.cpp`)

| Test | Description |
|---|---|
| `detectsTcpMsrpMediaBlock` | `TCP/MSRP` media line, `a=accept-types`, `a=path` → session-id, `a=setup`, `a=connection` all extracted |
| `detectsTcpTlsMsrpAndSessionId` | `TCP/TLS/MSRP` transport and session-id extraction from an `msrps://` relay path |
| `sdpWithoutMessageMediaIsNotPresent` | An audio-only SDP (`m=audio`) yields `present == false` |
| `audioOnlySdpIsNotPresent` | Empty input yields `present == false` |

Also exercised end-to-end (INVITE body → `MessagingTraceEntry.sdpMsrp`) by
`inviteWithMsrpSdpIsRelevant` in `tests/test_messaging_diagnostics_store.cpp`.

## Known Limitations

- Only the **first** `m=message` block is parsed; SDP bodies with multiple
  MSRP media lines only surface the first.
- No validation that `a=path`/`a=accept-types`/etc. are well-formed per
  RFC 4975/4976 — malformed values are still stored/displayed as-is (this is
  a diagnostics tool, not a conformance validator).
- Real MSRP session establishment (TCP/TLS connect, MSRP SEND/REPORT chunk
  framing) is **not implemented** anywhere in this codebase; this task adds
  only SDP-level detection.
