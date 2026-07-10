Lucrăm pe repo:
https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Task-W096 — IMDN Foundation

Branch:
feature/w096-imdn-foundation

Pleacă din:
feature/w095-deflate-rcs-diagnostics

Obiectiv:
Implementează suport complet pentru IMDN (RFC 5438) peste infrastructura construită în W090–W095. MSRP rămâne dezactivat.

Reguli:
- Nu hardcoda IP-uri, domenii, porturi, useri, parole sau URI-uri.
- Nu rupe audio/video/RTT existent.
- Nu activa MSRP.
- Nu implementa Presence sau XCAP.
- Nu face merge în main.
- Actualizează docs și project-status.

IMPORTANT:
Înainte de implementare, verifică dacă pipeline-ul actual de parsare folosește QString/toLatin1() pentru recuperarea body-ului SIP.

Dacă există riscul pierderii octeților originali:
- mută parsarea internă pe QByteArray/raw bytes;
- păstrează QString doar pentru UI și export text;
- schimbarea trebuie să fie transparentă pentru restul aplicației;
- aceasta face parte din W096 și NU reprezintă un task separat.

Cerințe:

1. IMDN generator
Implementează generarea documentelor IMDN:
- delivered
- displayed
- failed
- error

Respectă RFC 5438.

2. Auto Delivered
Dacă mesajul primit solicită delivery notification:
- generează automat IMDN delivered.

Config:
Auto Send Delivered IMDN
Implicit: ON

3. Displayed
Nu trimite automat displayed.

Adaugă:
- opțiune UI:
  Auto Send Displayed IMDN

Implicit:
OFF

Dacă este OFF:
- utilizatorul poate marca manual mesajul ca citit.

4. Message correlation
Corelează IMDN cu mesajul original prin:
- Message-ID

Actualizează istoricul:
Queued
Submitted
Delivered
Displayed
Failed
Error

5. Parser IMDN
Extinde parserul existent pentru:
- delivered
- displayed
- failed
- error
- forbidden
- processed (dacă apare)

6. UI
În Message History:
afișează statusul mesajului.

Exemplu:
✓ Submitted
✓✓ Delivered
👁 Displayed
⚠ Failed

UI trebuie să rămână responsiv.

7. MessagingEventStore
Actualizează MessagingEvent:
- generatedImdn
- receivedImdn
- correlatedMessageId
- deliveryState

Fără duplicare de logică.

8. Export JSON
Extinde schema existentă:
- generatedImdn
- receivedImdn
- correlatedMessageId
- deliveryState

Păstrează compatibilitatea cu schemaVersion 2 dacă modificările sunt doar aditive.
Crește schema doar dacă este absolut necesar.

9. Tests
Adaugă teste pentru:
- generate delivered
- generate displayed
- receive delivered
- receive displayed
- receive failed
- correlation by Message-ID
- duplicate IMDN
- invalid IMDN
- CPIM + IMDN
- UTF-8
- regresii W090–W095

Rulează build complet:
ENABLE_PJSIP=ON

Rulează:
ctest --output-on-failure

10. Docs
Adaugă:
- docs/imdn.md

Actualizează:
- docs/sip-message.md
- docs/messaging-event-store.md
- docs/windows-trace-json-export.md
- docs/project-status.md
- docs/agent-prompts/W096-imdn-foundation.md
- docs/agent-results/W096-imdn-foundation-result.md

Commituri recomandate:
- feat(imdn): implement imdn document generator
- feat(imdn): implement automatic delivered notifications
- feat(imdn): correlate imdn with message history
- feat(ui): display imdn delivery state
- test(imdn): add imdn interoperability tests
- docs(imdn): document imdn workflow

Flux obligatoriu:
1. git status
2. git branch
3. git pull --ff-only
4. git checkout feature/w095-deflate-rcs-diagnostics
5. git checkout -b feature/w096-imdn-foundation
6. implementează incremental
7. build complet
8. ctest --output-on-failure
9. actualizează documentația
10. git push -u origin feature/w096-imdn-foundation
11. fără merge în main

Raport final obligatoriu:
1. branch
2. branch de pornire
3. fișiere modificate
4. cum se generează IMDN
5. cum funcționează auto Delivered
6. cum funcționează Displayed
7. cum se face corelarea după Message-ID
8. modificările de infrastructură (dacă au fost necesare)
9. modificările în exportul JSON
10. ce rămâne neimplementat
11. teste rulate
12. limitări
13. commituri
14. git status
15. push status
