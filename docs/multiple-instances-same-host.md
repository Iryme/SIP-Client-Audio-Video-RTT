# Running Two Instances on the Same Windows Host (Task W109A)

For testing calls between two local accounts (e.g. "Alice" and "Bob")
without two machines. Each instance needs its own:

| Resource | How it's isolated | Flag / setting |
|---|---|---|
| Settings/profile store | `--config-dir` (or `SIPCLIENT_CONFIG_DIR`) redirects both `AppSettings` and `SipProfileManager` to a private `SIPClient.ini`/`SIPClientProfiles.ini` in that directory | `--config-dir <path>` |
| SIP local port | Already OS-assigned (`pj::TransportConfig{port=0}` in `SipManager::ensureTransport()`) — no action needed, was never the source of a collision | n/a |
| RTP/RTCP port range | `--rtp-port-start`/`--rtp-port-end` (or `SIPCLIENT_RTP_PORT_START`/`SIPCLIENT_RTP_PORT_END`), applied to the account's `mediaConfig.transportConfig` before registration | `--rtp-port-start <n> --rtp-port-end <n>` |

MSRP's listener port is already per-call/ephemeral (`AppSettings::msrpPortMode()`
defaults to "automatic" — OS-assigned), so no extra flag is needed there for
the same-host scenario; if `msrpPortMode` is set to `"fixed"`, give each
instance a different `msrpFixedPort` via its own `--config-dir` ini.

This app has no persistent log file (`Logger` is in-memory/signal-based — see
`docs/project-status.md`'s Known Limitations), so there is no separate
`--log-dir` to configure; each instance's Diagnostics panel/SIP Ladder is
already independent (in-process).

## Example (values below are illustrative, not hardcoded anywhere in code)

```
SIPClient.exe --config-dir C:\test\alice --rtp-port-start 4000 --rtp-port-end 4998
SIPClient.exe --config-dir C:\test\bob   --rtp-port-start 6000 --rtp-port-end 6998
```

or via environment variables (equivalent, CLI flags take priority if both
are set):

```
set SIPCLIENT_CONFIG_DIR=C:\test\alice
set SIPCLIENT_RTP_PORT_START=4000
set SIPCLIENT_RTP_PORT_END=4998
SIPClient.exe
```

Each instance then needs its own SIP profile added through its own Settings
page (profiles are per-`--config-dir`, so they start empty) — no
credentials or account details are assumed by this fix.

## Verified in this task

- Both instances start and run concurrently with distinct `--config-dir`/
  `--rtp-port-start`/`--rtp-port-end` — confirmed via a short smoke run
  (both processes alive after 5s, no crash, no immediate port conflict at
  startup).
- Full live scenario (REGISTER both, audio call, RTT request/accept,
  incremental T.140 exchange, hold/resume, hangup) requires a live SIP
  registrar and simultaneous interactive use of two GUI windows — **NOT
  RUN** in this session (no SIP test server/credentials were provided, and
  driving two interactive GUI windows in parallel is outside what this
  automated session can do). See the Faza 16 section of the task result
  report for the exact reason and what a human tester should do to
  complete it.
