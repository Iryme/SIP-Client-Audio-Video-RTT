# RTT Live Interoperability Test Procedure

**Scope**: End-to-end validation of RFC 4103 / T.140 real-time text between the
Iryme SIP client (Windows) and a compatible peer client or server.

**Status**: Partial live validation completed (2026-06-26) — Task 28.5.
SDP negotiation (m=text, RED level 2), RTT stream activation, audio media,
and hangup cleanup validated live against PJSUA 2.17-dev peer + Kamailio 5.x.
RTT text TX/RX character-by-character requires GUI manual test (see pass/fail
table). RTT must NOT be declared fully production-ready until scenario A and
B character-level tests pass.

---

## Prerequisites

### Kamailio / SIP server
- Kamailio 5.x or equivalent with `textops`, `sdpops` modules loaded.
- The server must NOT strip or alter the `m=text` SDP line.
- Confirm with: `kamailio -f /etc/kamailio/kamailio.cfg -c` — verify no SDP
  filtering rules affect `text` media.

### Windows client (Iryme)
- Branch `feature/project-handoff-002`, commit ≥ `bf9bae9`.
- Built in Release or Debug with `HAVE_PJSIP` defined.
- RTT panel visible in the call UI.
- Log level set to Debug (Settings → Log Level) so SDP and text stream lines
  appear in the log.

### Peer RTT client
One of:
- **Linphone** (desktop, 5.x) — enable RTT in call settings: Preferences →
  Calls → Real Time Text (RTT).
- **Blink** (Python SIP client) — supports RFC 4103 natively.
- **PJSUA CLI** (`pjsua --use-rtt`) — use `pjsua --help` for text options.
- Any SIP UA that sets `m=text` in SDP and sends `text/t140` RTP.

---

## SDP Verification Checklist

After call establishment, confirm these lines appear in the Iryme application log
(Settings → Log → filter on "SDP"):

```
SDP offer/answer m= lines (3): m=audio ..., m=video ..., m=text ...
PJSIP RTT text stream active: pjsipCallId=X mediaIndex=Y (RFC 4103 / T.140)
Negotiated text codec: red/90000  pt=Z  RED=yes (RFC 4103 / RFC 2198)
```

**If RED was NOT negotiated** (peer does not support red/90000):
```
Negotiated text codec: t140/1000  pt=Z  RED=no (plain T.140 only)
```
This is acceptable — fallback to plain T.140 is correct behaviour.

**RTT panel state** must show: `RTT: Active` (not "Not negotiated").

---

## Test Scenarios

### A. Windows → Peer: typing, backspace, Enter

1. Windows client dials the peer.
2. Call is answered. RTT panel on Windows shows `RTT: Active`.
3. Type "Hello" in the RTT input field on Windows.
   - **Expected on peer**: peer receives "H", "e", "l", "l", "o" as they are typed
     (char-by-char via T.140 RTP).
4. Press Backspace twice on Windows (deletes "lo").
   - **Expected on peer**: peer receives U+0008 × 2 and removes "lo" from its live
     display.
5. Type " World" (with space).
   - **Expected on peer**: peer live display shows "Hel World".
6. Press Enter / Send on Windows.
   - **Expected on peer**: peer receives T.140 CR (U+000D); text "Hel World" moves
     to transcript on peer. Windows transcript shows "You: Hel World".
   - Windows input field is cleared.

**PASS criteria**: Peer sees live character-by-character updates; backspace removes
chars on the peer; Enter triggers transcript flush.

---

### B. Peer → Windows: typing, backspace, Enter

1. Same call as above (still connected).
2. Peer types "Peer typing" into their RTT input.
   - **Expected on Windows**: `Remote typing:` area in RTT panel updates char by char.
3. Peer presses Backspace to delete "typing" (6 chars).
   - **Expected on Windows**: live area shrinks back to "Peer ".
4. Peer types "text" and presses Enter/CR.
   - **Expected on Windows**: "Peer text" moves from live area to transcript as
     "Remote: Peer text". Live area clears.

**PASS criteria**: Windows remote-live area updates in real time; backspace works;
Enter flushes to transcript.

---

### C. Simultaneous audio + video + RTT

1. Both clients in an active call with audio, video, and RTT negotiated.
2. Verify audio is bidirectional (voice audible in both directions).
3. Verify video renders in both directions.
4. Simultaneously: one party types RTT, the other speaks.
   - **Expected**: No audio dropout, no video freeze, RTT continues to flow.
5. Mute audio on Windows while typing RTT.
   - **Expected**: RTT unaffected by mute state.

**PASS criteria**: All three media streams active simultaneously with no
interference.

---

### D. Hangup and cleanup

1. While RTT is active (chars typed but not yet sent), hang up the call from
   Windows.
   - **Expected**: RTT input is disabled, live remote area cleared, state shows
     `RTT: Not negotiated`.
2. Initiate a new call to the same peer.
   - **Expected**: RTT panel starts clean (no residual text from previous call).
3. Hang up from the peer side.
   - **Expected**: Same cleanup as above.

**PASS criteria**: No residual state between calls; input correctly disabled after
hangup.

---

## Pass / Fail Record

| Scenario | Date | Peer client | Result | Notes |
|----------|------|-------------|--------|-------|
| A. Win→Peer SDP+RTT active | 2026-06-26 | PJSUA 2.17-dev / Kamailio 5.x | **PASS** | m=text offered, RTT Active, RED lvl=2 confirmed; char-level TX not tested via probe |
| A. Win→Peer char-by-char TX | — | — | **PENDING** | Requires GUI manual test |
| B. Peer→Win char-by-char RX | — | — | **PENDING** | Requires GUI manual test |
| C. Audio+Video+RTT simultan | 2026-06-26 | PJSUA 2.17-dev | **PARTIAL** | Audio G722 0% loss; video deactivated (no video codec in PJSIP build); RTT Active |
| D. Hangup cleanup | 2026-06-26 | PJSUA 2.17-dev | **PASS** | RTT Active→Disabled on BYE; 3×consecutive calls, no crash |

---

## Known Limitations

- **RED negotiation depends on peer**: If the peer does not advertise `red/90000`
  in its SDP m=text, PJSIP falls back to plain `t140/1000`. Both paths work.
- **Backspace (U+0008) on peer**: The peer must handle T.140 BS correctly. Some
  older UA implementations display U+0008 literally instead of removing the
  previous character.
- **ETSI/NG112 not implemented**: No MIME type multiplexing (text/t140 + MIME
  boundary); standard RFC 4103 only.
- **No RED level adjustment at runtime**: `redundancyLevel` is set at account
  creation time. Changing it requires re-registration.
