# Agent Prompt — Task W090: Messaging and MSRP Diagnostics Foundation

Verbatim task prompt as received, preserved for provenance.

---

Lucrăm pe repo:
https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Task-W090 — Messaging and MSRP Diagnostics Foundation

Branch obligatoriu:
feature/w090-msrp-lmpe-diagnostics

Reguli permanente:
- Nu hardcoda IP-uri, domenii, porturi, useri, parole sau URI-uri.
- Toate setările trebuie să vină din profiluri/config/UI.
- Nu rupe audio/video/RTT existent.
- Nu activa MSRP implicit.
- Tot experimental trebuie să aibă flag UI/config.
- Fiecare modificare trebuie documentată în docs/.
- Nu face merge în main.

Obiectiv:
Adaugă în client o fundație de diagnostic pentru SIP MESSAGE, CPIM, IMDN, is-composing și MSRP/SDP, fără să implementezi încă sesiuni MSRP reale.

Cerințe tehnice:
1. Creează o pagină/panel "Messaging Diagnostics".
2. Capturează și afișează SIP MESSAGE cu:
   - direction
   - From
   - To
   - Call-ID
   - CSeq
   - Content-Type
   - body preview sigur
   - timestamp
   - raw SIP redacted pentru export.
3. Detectează Content-Type:
   - text/plain
   - text/html
   - message/cpim
   - message/imdn+xml
   - application/im-iscomposing+xml.
4. CPIM:
   Parsează minimal:
   - From
   - To
   - DateTime
   - Subject
   - Content-Type.
5. IMDN:
   Detectează:
   - delivered
   - displayed
   - failed
   - error
   și extrage dacă există:
   - Message-ID
   - original-recipient
   - final-recipient.
6. is-composing:
   Detectează:
   - active
   - idle
   - gone
   și extrage:
   - timeout
   - refresh, dacă există.
7. SDP MSRP diagnostics:
   Detectează în SDP:
   - m=message
   - a=path
   - a=accept-types
   - a=setup
   - a=connection
   - TCP/MSRP
   - TCP/TLS/MSRP
   - session-id, dacă se poate extrage.
8. Adaugă "MSRP Diagnostics" read-only:
   - nu porni sesiuni MSRP reale.
   - doar detectează, loghează și afișează.
9. SIP Ladder:
   Asigură apariția în ladder pentru:
   - MESSAGE
   - CPIM
   - IMDN
   - is-composing
   - SDP cu m=message.
10. Export:
   Adaugă export JSON/TXT pentru Messaging Diagnostics.
   Include raw SIP redacted.
11. UI:
   - Nu înghesui call control.
   - Nu bloca UI thread.
   - Păstrează stabilitatea aplicației.
12. Tests:
   Adaugă teste unitare pentru:
   - CPIM parser
   - IMDN parser
   - is-composing parser
   - SDP MSRP parser

Dacă nu există infrastructură clară de teste, adaugă teste simple documentate și rulabile local.

Docs obligatorii:
- docs/messaging-diagnostics.md
- docs/msrp-diagnostics.md
- docs/project-status.md
- docs/agent-prompts/W090-msrp-lmpe-diagnostics.md
- docs/agent-results/W090-msrp-lmpe-diagnostics-result.md

Commituri recomandate:
- feat(messaging): add messaging diagnostics model
- feat(messaging): parse cpim imdn and is-composing
- feat(msrp): detect msrp sdp attributes
- feat(ui): add messaging diagnostics panel
- test(messaging): add parser validation tests
- docs(messaging): document diagnostics workflow

Flux obligatoriu:
1. Verifică starea repo:
   git status
   git branch
   git pull --ff-only
2. Creează branch:
   git checkout -b feature/w090-msrp-lmpe-diagnostics
3. Implementează incremental, cu commituri tematice.
4. Rulează build/teste.
5. Actualizează documentația.
6. Push branch la origin.
7. Nu face merge în main.

Raport final obligatoriu:
1. branch
2. fișiere modificate
3. ce parsează clientul
4. ce este doar diagnostic
5. ce NU este încă implementat
6. teste rulate
7. limitări
8. commituri
9. git status
10. push status
