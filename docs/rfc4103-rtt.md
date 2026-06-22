# RFC 4103 Real-Time Text (RTT)

**Status:** NOT STARTED

## Overview

RFC 4103 defines RTP payload format for T.140 text. Each character typed is transmitted in near-real-time, enabling real-time text communication alongside audio/video calls.

## Architecture (Planned)

```
RttSession
  ├── T140Buffer      — assembles received T.140 packets into text
  ├── T140Sender      — batches local keystrokes, sends at ~300ms intervals
  └── RttTransport    — wraps PJSIP RTP stream for T.140 payload
```

## UI Behavior

- RTT tab in right panel shows incoming real-time text character by character.
- "Remote typing" area shows the current incomplete sentence being received.
- Transcript shows completed exchange.
- Local input field sends characters as typed (every keystroke → RTP packet).

## T.140 Encoding

- UTF-8 text in RTP payload.
- Redundancy encoding (RFC 2198) for loss recovery — configurable.
- BOM and CR/LF handling per RFC 4103.

## SDP Negotiation

```
m=text 9 RTP/AVP 100
a=rtpmap:100 t140/1000
a=fmtp:100 cps=30
```

Redundancy:
```
a=rtpmap:101 red/1000
a=fmtp:101 100/100/100
```

## ETSI Compatibility

When ETSI TS 103 479 module is enabled, the RTT engine gains additional:
- Priority indication
- Session identity
- Emergency-specific session handling

Base RFC 4103 implementation must remain fully functional without ETSI module.

## Implementation Notes

- RTT operates on a separate RTP port from audio/video.
- `RttSession` lifecycle is tied to `SipCall` lifetime.
- All T.140 data is logged at `DEBUG` level; raw RTP at `RAW` level only.
- Incoming RTT character events are posted to the Qt main thread and appended to `RttPanel`.
