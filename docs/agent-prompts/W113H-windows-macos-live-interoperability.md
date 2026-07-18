# Agent Prompt — Task W113H: Windows 1.6.8 Parity Build and Live Windows ↔ macOS Interoperability Validation

Condensed from the full task spec provided by the project owner in chat — this
file summarizes the requirements rather than duplicating every line.

## Repository / branch

- Repository: `SIP-Client-Audio-Video-RTT`.
- New branch: `test/w113h-windows-macos-live-interoperability`.
- Starting branch (per spec): `feature/w113g-macos-arm64-bundle`. HEAD matched
  the required `e82a496` exactly at task start.
- No merge into `main`/`release`, no tag. `pjproject` not modified — confirmed
  by diffing `8fa1fb0..e82a496` (W113G touched only macOS bundle/keychain/docs
  code, no `.deps`/pjproject changes).

## Version

Already `1.6.8` at task start (bumped by W113G). No bump performed — the
spec's own ceiling ("do not exceed 1.6.8 unless a real approved bugfix
requires it") was respected as-is.

## Hard environment constraint

This session runs on a single Windows 11 machine with no macOS device, no
second Windows machine, and no SIP test server/accounts reachable from it.
The task's Phases 5–13 (live Windows↔macOS SIP/audio/video/RTT/messaging/
presence/MSRP interoperability) require physical access to both platforms and
a live server simultaneously — none of which exist in this environment.

Per explicit user instruction (asked directly before starting, given the
task's own "never declare PASS without a real test" rule), scope was set to:
**everything achievable on Windows alone**, with every macOS-dependent phase
reported honestly as BLOCKED rather than fabricated, simulated, or inferred
from unit tests.

## What was executed

1. Faza 1 — branch sync: fetch, checkout `feature/w113g-macos-arm64-bundle`,
   verify HEAD/clean tree, create the new branch.
2. Faza 2 — Windows environment inventory (OS, MSVC toolset, Windows SDK,
   CMake, Qt, PJSIP, OpenSSL-in-PJSIP).
3. Faza 3 — Debug and Release configure+build+CTest, `ENABLE_PJSIP=ON`, full
   81-test suite including the LMPE-disabled and message-routing/raw-XML
   regression tests, on both `build/` and `build-release/`.
4. Faza 4 — rebuilt the packaging tree (`build-windows-x64-release`) at
   1.6.8 and ran `scripts/package-windows.ps1` end-to-end: Qt deployment,
   VC++ runtime bundling, non-Qt dependency audit, secret/local-path scan,
   architecture/Debug-DLL check, smoke test, ZIP+SHA-256+manifest.
5. Fazele 5–13 — reported BLOCKED (see result doc), not executed.
6. Faza 15 — this prompt, the paired result doc, and updates to
   `project-status.md`, `windows-macos-interoperability-test.md`, and new
   `known-limitations.md`/`version-matrix.md` (both did not previously exist
   in this repository).

## Global rules (same discipline as every task this session)

No pjproject changes; no protocol additions; no merge; no tag; PASS only for
things actually executed and observed; BLOCKED/NOT RUN/UNSUPPORTED reported
honestly and explicitly for everything that wasn't.

See [W113H-windows-macos-live-interoperability-result.md](../agent-results/W113H-windows-macos-live-interoperability-result.md)
for the full result and feature matrix.
