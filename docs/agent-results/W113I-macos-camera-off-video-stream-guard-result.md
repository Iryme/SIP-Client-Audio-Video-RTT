# W113I Result — macOS Camera Off Video Stream Guard

Date: 2026-07-19  
Branch: `fix/w113i-macos-camera-off-video-stream-guard`  
Base: `feature/w113g-macos-arm64-bundle` at `e82a49679225d9b039477889c05d4692bc543aba`  
Artifact source commit: `bbb08c67c22b0217d043c9c38366524e58ba68f3`

## Outcome

The application-level guard is implemented without pjproject changes. macOS
application/backend/UI/bundle metadata is 1.6.9/build 169. Windows remains
1.6.8. Native ARM64 Debug and Release builds and 83/83 tests in each
configuration pass. The ad-hoc signed ZIP/DMG were generated and the bundle
passed deployment, relocation, dependency, RPATH, architecture, signature,
and smoke checks.

Live post-fix REGISTER/call testing is BLOCKED: this host has no SIP test
server/accounts/reference peer, and its PJSIP build has no video codec. No live
audio-only/no-common-codec/video result is inferred from automated tests.

## Incident and root cause

- Crash ID: `11B9322A-6619-4543-B61D-694510267BD4`.
- Archived pre-fix reproduction: PASS. The crash report is from SIP Client
  1.6.8/build 168 launched at
  `/Volumes/VOLUME/SIP Client.app/Contents/MacOS/SIP Client` on macOS 26.5.2
  (25F84), Mac14,9 / Apple M2 Pro, native ARM64.
- Result: main-thread `EXC_CRASH/SIGABRT` at
  `pjmedia_vid_stream_pause → pjsua_call_set_vid_strm →
  pj::Call::vidSetStream → SipCall::setVideoMuted →
  SipManager::setCallVideoMuted → CameraController::enabledChanged`.
- Exact invalid state proved by source and stack: the old application obtained
  a non-negative media index (otherwise its own branch could not reach
  `vidSetStream`), but PJSIP's `call_med->strm.v.stream` was null/unprepared.
  PJSIP 2.17 checks media type (and direction for resume) but did not check that
  pointer before passing it to `pjmedia_vid_stream_pause`, whose first action
  is an assertion that the stream pointer is non-null.
- Call state/media count/video count/media status/media index values were not
  embedded in the Apple crash report and no pre-crash application log was
  available. They are therefore UNKNOWN, not fabricated. The known media index
  property is “non-negative but referring to a slot with no stream object.”
- AVFoundation, Metal, and Qt Multimedia are not on the crashing stack.

## Guard and UI behavior

Before STOP/START_TRANSMIT, `SipCall` now requires all of the following:

1. PJSIP call object and valid call ID;
2. app state outside Idle/Failed/Disconnecting and PJSIP state outside
   NULL/DISCONNECTED;
3. an explicitly located video `CallMediaInfo` entry (never assumed index 0);
4. media index matching `pjsua_call_get_vid_stream_idx()`;
5. ACTIVE media status and encoding/transmit direction;
6. successful `pjsua_call_get_stream_info()` returning video, which verifies
   the internal stream object under PJSUA's lock.

If any check fails, no `vidSetStream` call occurs. Camera Off remains a benign
local-preview stop through `CameraController`/`VideoPanel`; Camera On may manage
local preview but does not resume or declare active call video. No absent media
is reported as remotely muted. Applied media-index tracking makes repeated
operations idempotent and resets when the video bridge is torn down. A
`pj::Error` around call inspection or the final operation logs status, title,
reason, call state, and media index without aborting.

## Automated validation

| Gate | Result |
|---|---|
| Native macOS ARM64 Debug build | PASS |
| Debug CTest | PASS — 83/83 |
| Native macOS ARM64 Release build | PASS |
| Release CTest | PASS — 83/83 |
| No call / invalid call ID | PASS — guard policy |
| Audio-only / video absent | PASS — guard policy |
| Inactive/error/held video | PASS — guard policy |
| Invalid/mismatched media index | PASS — guard policy |
| Null internal PJSIP stream object | PASS — rejected before operation |
| Disconnected/teardown/post-BYE model | PASS — guard policy |
| Repeated Camera On/Off decision | PASS — stable/idempotent policy |
| `vidSetStream` absent on rejected state | PASS — fake operation count remains zero |
| Valid active transmitting video | PASS — only accepted policy state |
| LMPE disabled regression | PASS — `test_lmpe_codec_unconfirmed` |
| Client Messaging raw XML regression | PASS — conversation/CPIM/IMDN/is-composing suites |

The tests deliberately do not invoke the real invalid PJSIP operation because
that path asserts rather than returning an error.

## Live macOS matrix

| Scenario | Result |
|---|---|
| Pre-fix Camera Off crash | PASS (reproduced archived incident above) |
| No-call Camera On/Off x10 and restart | NOT RUN — interactive GUI/camera permission gate unavailable in unattended run |
| Audio-only Camera On/Off, hold/resume, BYE, post-BYE | BLOCKED — no SIP peer/server/accounts |
| Video offer with no common codec | BLOCKED — no SIP peer/server/accounts |
| Granted/denied/revoked camera permission | NOT RUN — interactive OS privacy UI |
| Camera toggle concurrent with remote BYE/teardown/shutdown | BLOCKED — no remote peer |

## Windows follow-up required

Classification: **B/C — common code with potentially functional Windows
camera-toggle impact**. Common files are `src/sip/SipCall.cpp`,
`src/sip/VideoStreamGuard.*`, `CMakeLists.txt`, and test configuration. No
macOS-only runtime adapter changed. Windows Debug/Release/live regression is
NOT RUN/BLOCKED because no Windows host/VM is available; it is not PASS.

Windows remains 1.6.8 in `PROJECT_VERSION`, `AppVersion.rc.in`, About/runtime
reporting, and packaging. Proposed Windows version after separate validation:
1.6.9. Recommended branch:
`fix/w113i-windows-video-stream-guard-validation`. The executable follow-up
prompt is
[W113I-windows-video-stream-guard-follow-up.md](../agent-prompts/W113I-windows-video-stream-guard-follow-up.md).

## Bundle and audits

| Item | Result |
|---|---|
| Info.plist | PASS — 1.6.9/build 169, minimum macOS 13.0 |
| version-info.json | PASS — application/backend/UI 1.6.9 |
| macdeployqt | PASS |
| Architecture | PASS — 28 Mach-O files, all ARM64, zero x86_64 |
| Dependencies | PASS — system or bundle-relative only |
| RPATH | PASS — no developer/local paths |
| Secret/local-path scan | PASS |
| Signing | PASS — ad-hoc deep/strict verification |
| Gatekeeper/notarization | BLOCKED/NOT RUN — no Developer ID/notary profile; ad-hoc app is expected to be rejected |
| Original bundle smoke | PASS — SIP Client 1.6.9 |
| Relocated bundle smoke | PASS — SIP Client 1.6.9 |
| dSYM/symbol ZIP | NOT PRODUCED — no dSYM generated |

- ZIP: `SIP-Client-Audio-Video-RTT-1.6.9-macos-arm64.zip`, 24,172,153 bytes,
  SHA-256 `8d544176fbea59aa383a3f5739c40af65edb6eb670d6fa158bc62c579dc7d09b`.
- ZIP sidecar: verified PASS from the artifact directory.
- DMG: `SIP-Client-Audio-Video-RTT-1.6.9-macos-arm64.dmg`, 26,969,403 bytes,
  SHA-256 `93f88695efe7afed0a1f36275d22279189dae0411f61ac38aa093a637ad6f737`.
- Manifest: `SIP-Client-Audio-Video-RTT-1.6.9-macos-arm64-manifest.json`.

## Final status

PASS: implementation, exact stream-object guard, regression tests,
Debug/Release builds, package/audits/signing/smoke, macOS version separation,
LMPE and Client Messaging regressions.

FAIL: none observed.

BLOCKED/NOT RUN: post-fix live SIP/camera privacy matrix, Windows rebuild and
regression, Developer ID/notarization, second clean Mac.

No merge and no tag were created. No pjproject source was modified. No codec,
credential, local developer path, or environment-specific value was added to
tracked source or artifacts.
