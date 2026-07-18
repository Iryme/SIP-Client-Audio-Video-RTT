# Agent Prompt — Video Latency & Framerate Investigation

Ad-hoc task, not part of the numbered W1xx roadmap. Requested directly by
the project owner in chat, before starting Task W113E, after reviewing a
manual Alice/Bob test session: "am log-uri de la o sesiune de testare,
sunt probleme cu video si framerate ;a video, de asemenea latency este
foarte mare in conditia in care sunt in retele loacale" (I have logs from
a test session, there are problems with video and framerate, also latency
is very high given that we're on local networks).

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `fix/video-latency-and-codec-investigation`.
- Starting branch: `release/w113d-portable-windows-bundle` (W113D,
  complete, 80/80 CTest, pushed without merge).
- No merge into `main`/`release`. `pjproject` sources not modified.

## Objective

Given two Diagnostics Center export bundles from the reported test
session (`diagnostics-20260718-213203.zip`, `diagnostics-20260718-213221.zip`,
both app v1.6.4), determine the root cause(s) of the reported video/
framerate/latency problems before proceeding with W113E (which was
explicitly paused pending this investigation — the user chose "investigate
now, as its own task first" over the alternative of deferring or folding
a minimal fix into W113E, when asked).

## Approach

1. Unzip and read both bundles' `logs.txt`, `diagnostics.json`,
   `sip_trace.json`, `version.txt`.
2. Trace every video-codec-negotiation and video-window-attach log line
   back to the source code that emits it.
3. Trace the Call Workspace's FPS/Video-Drops status cards and the
   diagnostics export's video fields back to their actual data sources.
4. For anything that looked like a bug, verify against the code before
   concluding it was one (several apparent anomalies — repeated
   video-window re-attachment, one RTT negotiation timeout — turned out to
   be expected behavior once cross-referenced against surrounding log
   context).
5. Fix what's small, real, and low-risk; document what needs a dedicated
   follow-up task instead of rushing a larger change into an investigation
   task.

## Global rules (same discipline as every task this session)

Mandatory version bump (1.6.4 → 1.6.5, PATCH); Debug + Release +
`ENABLE_PJSIP=ON` builds; all existing tests must still pass; docs;
`docs/project-status.md` update; agent-prompt/agent-result pair; push (no
merge); no pjproject changes.

See [video-latency-and-codec-investigation-result.md](../agent-results/video-latency-and-codec-investigation-result.md)
for the full findings and report.
