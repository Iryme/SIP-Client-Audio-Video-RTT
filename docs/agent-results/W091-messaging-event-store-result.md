# Agent Result — Task W091: Messaging Event Model + Safe Message Store

## 1. Branch

`feature/w091-messaging-event-store`, branched from `feature/w090-msrp-lmpe-diagnostics`
(W090 is not yet integrated into a release branch, per the task's own
fallback instruction). Not merged into `main` (no `main` branch exists in
this repo; not merged into anything).

## 2. Files modified / added

### Modified
- `CMakeLists.txt` — new source/header entries for `MessagingEvent.h`, `MessagingEventStore.h/.cpp`
- `tests/CMakeLists.txt` — `MessagingEventStore.cpp` added to `MESSAGING_DIAGNOSTICS_SOURCES`; new `test_messaging_event_store` target
- `src/core/AppSettings.h` — `loadMaxMessagingEventsRetained()` / `saveMaxMessagingEventsRetained(int)`, key `messaging/maxEventsRetained`, default 1000
- `src/gui/panels/MessagingDiagnosticsPage.h` / `.cpp` — now sourced from `MessagingEventStore` instead of `MessagingDiagnosticsStore`; new "Parse" column for parse warnings
- `src/gui/panels/SettingsPanel.h` / `.cpp` — new "Messaging Diagnostics" group (Max events retained spin box) in the Text/Accessibility tab
- `docs/messaging-diagnostics.md` — updated architecture diagram/description to reflect the new event-store layer
- `docs/project-status.md` — task entry, module table rows, header date/branch/task count

### Added
- `src/sip/MessagingEvent.h` — the unified `MessagingEvent` model
- `src/sip/MessagingEventStore.h` / `.cpp` — the store (append/clear/count/snapshot/export, mapping, retention limit)
- `tests/test_messaging_event_store.cpp` — 7 test functions
- `docs/messaging-event-store.md`
- `docs/agent-prompts/W091-messaging-event-store.md`
- `docs/agent-results/W091-messaging-event-store-result.md` (this file)

## 3. The `MessagingEvent` model introduced

Header-only struct (`src/sip/MessagingEvent.h`), all fields requested:

`id` (qint64, sequential, assigned by the store), `timestamp`, `direction`
(`Inbound`/`Outbound`/`Unknown`), `transport` (`SipMessage`/`Msrp`/`Unknown`),
`payloadType` (`Plain`/`Html`/`Cpim`/`Imdn`/`IsComposing`/`Sdp`/`Unknown`),
`from`, `to`, `callId`, `cseq`, `contentType`, `bodyPreview`,
`rawSipRedacted`, `parseStatus` (`Ok`/`Partial`/`Error`), `parseWarnings`
(`QStringList`). Each enum has a `static ...ToString()` helper matching the
project's existing convention (`SipMessageTrace`, `ImdnInfo`, etc.).

## 4. How W090 is mapped into the new store

`MessagingEventStore` connects to `MessagingDiagnosticsStore::entryLogged`
(the existing W090 signal) and, for each `MessagingTraceEntry`, calls the
`static` `mapFromTraceEntry()` function, which:
- Copies base fields (timestamp, direction, from/to/callId/cseq/contentType, bodyPreview, rawSip → rawSipRedacted).
- Sets `transport = SipMessage` when `method == "MESSAGE"`, `Msrp` when an SDP `m=message` line was detected (`sdpMsrp.present`), else `Unknown`.
- Maps `MessagingContentKind` 1:1 to `MessagingEvent::PayloadType`.
- Sets `parseStatus = Partial` and appends a human-readable warning when a declared Content-Type's structured parse came back empty (CPIM/IMDN/is-composing/SDP-MSRP transport token), or when `Call-ID` is missing.

No CPIM/IMDN/is-composing/SDP-MSRP parsing is reimplemented — `MessagingEventStore` only reads fields already parsed by W090's `CpimParser`/`ImdnParser`/`IsComposingParser`/`SdpMsrpDiagnosticsParser` via `MessagingTraceEntry`.

Compatibility with the existing panel is preserved: `MessagingDiagnosticsPage`
still resolves the full structured CPIM/IMDN/is-composing/SDP-MSRP detail on
row double-click, by looking up `MessagingDiagnosticsStore::instance().entries()`
at index `event.id - 1` (both stores are cleared together from the single
Clear button, keeping id numbering aligned with that list's order — see
`docs/messaging-event-store.md` for the exact contract).

## 5. What remains diagnostic-only

- No real MSRP session is ever opened by this task — `transport = Msrp` is a
  classification label for an *offered/observed* MSRP negotiation, not a
  live connection.
- No SIP MESSAGE is sent by this code — `MessagingEventStore` is a pure
  observer of `MessagingDiagnosticsStore`'s already-captured traces.
- The Settings "Max events retained" field only bounds an in-memory
  diagnostics list; it has no effect on SIP/MSRP transport behavior.
- Existing audio/video/RTT/registration/call code paths were not touched.

## 6. What is NOT yet implemented

- No new CPIM/IMDN/is-composing/SDP-MSRP parsing (out of scope — that is Task W090's domain, reused as-is).
- No real MSRP transport, no sending/composing of SIP MESSAGE, CPIM, IMDN, or is-composing bodies.
- No persistence of `MessagingEventStore`'s list across app restarts (matches `MessagingDiagnosticsStore` behavior).
- No bundle-export integration (`DiagnosticsBundleExporter`) — still standalone Export Text/JSON buttons.
- `MessagingDiagnosticsStore`'s own list is not itself bounded by the new retention limit — only `MessagingEventStore`'s list is; see the "id alignment" caveat in `docs/messaging-event-store.md`.

## 7. Tests run

Built and ran on Windows with MSVC 2026 (NMake Makefiles generator) against
the `build` tree, configured with `ENABLE_PJSIP=ON` and `BUILD_TESTS=ON`:

```
cmake . && nmake && ctest --output-on-failure
```

Result: **39/39 tests passed**, 0 failed — all 38 pre-existing tests (no
regressions, including the 5 W090 suites) plus the 1 new suite added by this
task:

| Test | Result |
|---|---|
| test_messaging_event_store (7 test functions) | Passed |

## 8. Limitations

- `parseStatus` currently only distinguishes `Ok`/`Partial`; `Error` is
  defined but not produced, since the underlying W090 parsers are
  tolerant/best-effort and do not currently surface hard parse errors.
- The `id - 1` alignment between `MessagingEventStore` and
  `MessagingDiagnosticsStore` (used to resolve structured detail on row
  double-click) depends on both stores only ever being cleared together via
  the single Clear button; a hypothetical future caller that clears one
  store without the other would desynchronize the mapping.
- `MessagingEvent::transport` does not currently distinguish "SDP offer for
  MSRP that was accepted" from "offered but rejected/never negotiated" —
  it only reflects that an `m=message` line was observed.
- Same content-parsing limitations as W090 (IMDN opportunistic recipient
  fields, one-level CPIM unwrapping, first-`m=message`-block-only SDP
  parsing) — unchanged, see `docs/messaging-diagnostics.md`.

## 9. Commits

Thematic commits on this branch (see `git log feature/w091-messaging-event-store`):

1. `feat(messaging): add unified messaging event model` — `MessagingEvent.h`, CMakeLists.txt entries
2. `feat(messaging): add safe messaging event store` — `MessagingEventStore.h/.cpp` (append/clear/count/snapshot/export/limit + W090 mapping), `AppSettings` max-events setting, tests/CMakeLists.txt target
3. `feat(ui): wire messaging diagnostics to event store` — `MessagingDiagnosticsPage` rewired, parse-warning column, `SettingsPanel` max-events spin box
4. `test(messaging): add messaging event store tests` — `test_messaging_event_store.cpp`, 7 tests passing
5. `docs(messaging): document event store workflow` — this commit's docs (see below)

## 10. git status

Clean — all listed files committed on `feature/w091-messaging-event-store`.

## 11. Push status

Pushed to `origin/feature/w091-messaging-event-store`. Not merged into `main`
(per instructions).
