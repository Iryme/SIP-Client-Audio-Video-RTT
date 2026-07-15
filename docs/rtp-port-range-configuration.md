# RTP Port Range Configuration (Task W109A)

## Problem

pjsua2 has no `EpConfig::MediaConfig` field for the local RTP/RTCP port
range (verified against the vendored headers in
`.deps/pjproject/pjsip/include/pjsua2/endpoint.hpp` — `pj::MediaConfig` only
covers clock rate, jitter buffer, echo cancellation, etc.). The actual pjsua2
API for this is **per-account**: `pj::AccountConfig::mediaConfig` (type
`AccountMediaConfig`) → `.transportConfig` (type `pj::TransportConfig`,
`port`/`portRange` fields — see `pjsip/include/pjsua2/siptypes.hpp`). Before
this task, the app never set this field at all, so pjsua2 fell back to its
own built-in default RTP base port (starting at 4000) for every account,
regardless of host. Running two instances of this app on the same Windows
machine meant both tried to bind the same port first — the observed
`WSAEADDRINUSE` when accepting an RTT request mid-call.

## Fix

`SipAccount::startRegistration()` now resolves an effective RTP port range
and applies it to `config.mediaConfig.transportConfig.port` /
`.portRange` before `account->create()` (i.e. before the account, and any
call made through it, ever touches the network) — see
`src/sip/RtpPortRangeConfig.h/.cpp`.

Resolution order (`resolveEffectiveRtpPortRange()`):
1. **Session-only CLI override** — `--rtp-port-start`/`--rtp-port-end` (or
   `SIPCLIENT_RTP_PORT_START`/`SIPCLIENT_RTP_PORT_END` env vars), parsed in
   `main.cpp` before `Application` is constructed. Never persisted.
2. **Persisted setting** — `AppSettings::rtpPortRangeStart()`/
   `rtpPortRangeEnd()` (`media/rtpPortStart` / `media/rtpPortEnd` in
   `SIPClient.ini`).
3. **Default** — `4000`–`4998` (matches the starting port pjsua2's own
   default would otherwise pick, but bounded — a single instance's own
   behavior is unaffected).

## Validation (`validateRtpPortRange`)

Rejected outright (falls back to the pjsua2 default, unbounded, with a
warning logged):
- start/end outside `[1024, 65535]`
- `start > end`
- `start` odd (RTCP is allocated at `start + 1` by convention)
- fewer than 16 usable ports in the range (not enough for one call's
  audio+video+text RTP/RTCP pairs plus headroom — silently allowing this
  would just move the collision from "instance vs instance" to
  "stream vs stream" within the same call)

Non-fatal warning (still applied) when the range has fewer than 40 ports —
tight, but usable for a single call at a time (this app supports one active
call).

## Same-host usage

See [multiple-instances-same-host.md](multiple-instances-same-host.md) for
the full Alice/Bob example. In short:

```
SIPClient.exe --config-dir <alice-dir> --rtp-port-start 4000 --rtp-port-end 4998
SIPClient.exe --config-dir <bob-dir>   --rtp-port-start 6000 --rtp-port-end 6998
```

Any two non-overlapping ranges work; these are examples, not hardcoded
values — nothing in the fix assumes a specific base port.

## What this does NOT change

- SIP signaling transport port: already `config.port = 0` (OS-assigned
  ephemeral) in `SipManager::ensureTransport()` — this was never the source
  of the collision and is unaffected.
- Audio/video/text negotiation logic — this only bounds *which* ports
  pjsua2 picks from, not how many streams or their SDP content.
- pjproject sources — not modified. The fix uses only the public
  `pj::AccountConfig`/`pj::TransportConfig` API.
