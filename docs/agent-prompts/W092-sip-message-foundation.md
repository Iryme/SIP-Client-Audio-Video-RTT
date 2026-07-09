# Agent Prompt — Task W092: SIP MESSAGE Foundation

Verbatim task prompt as received, preserved for provenance.

---

Task-W092 — SIP MESSAGE Foundation

Branch:
feature/w092-sip-message-foundation

Pleacă din:
feature/w091-messaging-event-store

Nu pleca din main/release, deoarece W090/W091 nu sunt încă integrate.

Obiectiv:
Adaugă suport de bază pentru trimitere și primire SIP MESSAGE în clientul Windows, folosind infrastructura W090/W091. MSRP rămâne complet dezactivat.

Reguli:
- Nu hardcoda IP-uri, domenii, porturi, useri, parole sau URI-uri.
- Toate destinațiile trebuie introduse din UI/profil/config.
- Nu rupe audio/video/RTT existent.
- Nu activa MSRP.
- Nu implementa sesiuni MSRP.
- Nu dubla parsing-ul din W090.
- Folosește MessagingEventStore din W091.
- Actualizează docs și project-status.
- Nu face merge în main.

Cerințe:

1. UI — pagină/panel Messaging
   Adaugă o zonă simplă pentru SIP MESSAGE:
   - destinatar SIP URI
   - Content-Type selectabil:
     - text/plain
     - text/html
     - message/cpim
   - editor mesaj
   - checkbox Enable SIP MESSAGE
   - checkbox Enable CPIM
   - checkbox Request IMDN
   - buton Send
   - istoric simplu inbound/outbound

2. Trimitere SIP MESSAGE
   Implementează trimiterea prin PJSIP/pjsua2:
   - MESSAGE către URI-ul introdus
   - Content-Type corect
   - body UTF-8
   - fără MSRP
   - fără hardcodări
   - fără blocarea UI thread

3. Primire SIP MESSAGE
   Mesajele primite trebuie să ajungă în:
   - Messaging Diagnostics
   - MessagingEventStore
   - istoricul Messaging
   - SIP Ladder

   Nu reimplementa parsing-ul. Refolosește pipeline-ul W090/W091.

4. CPIM generation
   Dacă utilizatorul alege message/cpim:
   - generează automat wrapper CPIM minimal
   - nu cere userului să scrie headerele manual

   Header minim:
   - From
   - To
   - DateTime
   - Content-Type

   Body-ul introdus de user devine payload CPIM.

5. IMDN request
   Dacă Request IMDN este activ:
   - adaugă doar headerele/metadata necesare pentru solicitare IMDN.
   - nu genera încă IMDN delivered/displayed automat.
   - nu implementa încă retry/offline logic în client.

6. Config/AppSettings
   Adaugă setări:
   - enableSipMessage
   - enableCpim
   - requestImdnByDefault

   Valorile default trebuie să fie conservatoare:
   - SIP MESSAGE dezactivat sau activ doar dacă UI îl pornește explicit
   - CPIM dezactivat implicit
   - IMDN request dezactivat implicit

7. Siguranță UI
   - Send dezactivat dacă SIP MESSAGE nu este enabled.
   - Send dezactivat dacă URI-ul este gol.
   - Send dezactivat dacă body-ul este gol.
   - Erorile trebuie afișate în UI/log, nu prin crash.
   - Nu bloca UI thread.

8. History
   Istoric simplu:
   - timestamp
   - direction
   - recipient/sender
   - content-type
   - body preview
   - send status dacă este outbound

   Nu este nevoie de conversații multiple încă.

9. Tests
   Adaugă teste pentru:
   - build SIP MESSAGE text/plain
   - build SIP MESSAGE text/html
   - build CPIM wrapper
   - UTF-8 body
   - body mai mare de 4KB
   - Request IMDN headers/metadata
   - invalid empty destination
   - invalid empty body
   - mapping outbound message -> MessagingEvent

10. Docs obligatorii:
   - docs/sip-message.md
   - update docs/messaging-diagnostics.md
   - update docs/messaging-event-store.md
   - update docs/project-status.md
   - docs/agent-prompts/W092-sip-message-foundation.md
   - docs/agent-results/W092-sip-message-foundation-result.md

Commituri recomandate:
- feat(messaging): add sip message composer model
- feat(messaging): implement sip message sender
- feat(messaging): generate cpim wrapper for outbound messages
- feat(ui): add sip messaging panel and history
- test(messaging): add sip message foundation tests
- docs(messaging): document sip message workflow

Flux obligatoriu:
1. Verifică:
   git status
   git branch
   git pull --ff-only
2. Creează branch din W091:
   git checkout feature/w091-messaging-event-store
   git pull --ff-only
   git checkout -b feature/w092-sip-message-foundation
3. Implementează incremental.
4. Rulează build complet.
5. Rulează CTest.
6. Actualizează docs.
7. Commituri tematice.
8. Push:
   git push -u origin feature/w092-sip-message-foundation
9. Nu face merge în main.

Raport final obligatoriu:
1. branch
2. branch de pornire
3. fișiere modificate
4. ce poate trimite clientul
5. ce poate primi clientul
6. cum se generează CPIM
7. ce face Request IMDN
8. ce rămâne neimplementat
9. ce este încă diagnostic-only
10. teste rulate
11. limitări
12. commituri
13. git status
14. push status
