# Windows Clean-Machine Test Procedure

Manual validation of a portable bundle produced by
[scripts/package-windows.ps1](../scripts/package-windows.ps1), run on a
machine that never had this project's build environment (no Qt, no Visual
Studio, ideally no prior SIPClient install at all) — a clean VM, Windows
Sandbox, or a second physical machine. This is what actually proves the
bundle is portable; the in-session smoke test (staging exe, minimal `PATH`,
same machine) only proves the *dependency closure* is complete, not that a
genuinely different machine can run it.

Copy only the `.zip` and its `.sha256` to the clean machine — nothing else.

## Steps

1. Verify SHA-256 matches the `.sha256` file shipped alongside the zip.
2. Unzip.
3. Launch `SIPClient.exe`.
4. Confirm no missing-DLL / missing-Qt-plugin error dialog appears.
5. Open Settings — confirm it opens and no SIP account/credential is
   pre-populated (bundle ships with no default account).
6. Configure a test SIP profile; REGISTER against a reachable test server.
7. Place/receive an audio call.
8. Open camera preview; confirm it starts only when explicitly requested
   (no unrequested camera activation).
9. Camera Off — confirm the camera LED actually turns off (see the W113c
   fix in [release-notes.md](release-notes.md); this is the regression this
   step exists to catch).
10. Camera On — confirm preview resumes.
11. Place/receive a video call.
12. Exercise RTT (RFC 4103) on a call.
13. Send/receive a SIP MESSAGE.
14. Exercise Presence, if the test server supports it.
15. Open Tools (SIP Ladder, diagnostics pages).
16. Run a Diagnostics Center export; confirm the exported bundle's
    `version-info`/version fields report this build's version, not a stale
    or mismatched one.
17. Shut down the app (clean close, not a task-kill).
18. Restart the app.
19. Confirm settings/profile from step 6 persisted across the restart.
20. Confirm the app created its config only under the current user's normal
    settings location (registry `HKEY_CURRENT_USER\Software\SIPClient` by
    default) — not inside the unzipped folder, not in `Program Files`, and
    without any admin-elevation prompt at any point above.

## Reporting

Report each step strictly as one of: **PASS**, **FAIL**, **BLOCKED**,
**NOT RUN**, or **UNSUPPORTED** (e.g. Presence against a server that doesn't
implement it). Never report PASS for a step that wasn't actually executed —
if no clean VM/second machine was available for a given packaging run, the
whole clean-machine pass is **NOT RUN**, and the corresponding
`docs/agent-results/*-result.md` must say so explicitly rather than imply
the bundle was fully validated.
