Lucrăm pe repo:
https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Task-W097 — Active is-composing

Branch:
feature/w097-is-composing

Pleacă din:
feature/w096-imdn-foundation

Obiectiv:
Implementează suport complet pentru RFC 3994 (application/im-iscomposing+xml) peste infrastructura existentă. MSRP rămâne dezactivat.

Reguli:
- Nu hardcoda IP-uri, domenii, porturi, useri, parole sau URI-uri.
- Nu rupe audio/video/RTT.
- Nu activa MSRP.
- Nu implementa Presence sau XCAP.
- Nu face merge în main.
- Actualizează docs și project-status.

Cerințe:

1. Generator is-composing
Implementează generator pentru:
- active
- idle
- gone

Include:
- refresh
- contenttype (dacă este cunoscut)

Respectă RFC 3994.

2. Trimitere automată

În editorul Messaging:

la începutul tastării:

trimite:
active

după perioada configurabilă de inactivitate:

trimite:
idle

la închiderea conversației sau oprirea editorului:

trimite:
gone

Nu trimite duplicate inutile.

3. Rate limiting

Nu trimite:

active
active
active
active

Introduce debounce și throttling.

Config:

typingRefreshSeconds

typingIdleSeconds

typingGoneDelay

4. Recepție

Mesajele primite trebuie să actualizeze:

Message History

MessagingEventStore

Messaging Diagnostics

fără duplicare de logică.

5. UI

În istoric afișează:

✍️ typing...

Idle

Gone

Indicatorul trebuie să dispară automat după refresh dacă nu mai vin notificări.

6. Export JSON

Extinde exportul:

generatedIsComposing

receivedIsComposing

typingState

typingRefresh

typingTimeout

Nu modifica schema decât dacă este absolut necesar.

7. Config

Adaugă:

Enable is-composing

Auto typing notifications

Implicit:

ON

8. Tests

Adaugă teste pentru:

active

idle

gone

refresh timer

debounce

duplicate suppression

receive active

receive idle

receive gone

timeout expiration

JSON export

regresii W090-W096

Rulează:

ENABLE_PJSIP=ON

ctest --output-on-failure

9. Docs

Adaugă:

docs/is-composing.md

Actualizează:

docs/sip-message.md

docs/messaging-event-store.md

docs/windows-trace-json-export.md

docs/project-status.md

docs/agent-prompts/W097-is-composing.md

docs/agent-results/W097-is-composing-result.md

Commituri recomandate:

feat(is-composing): implement notification generator

feat(is-composing): automatic typing state machine

feat(ui): typing indicator

test(is-composing): add interoperability tests

docs(is-composing): document workflow

Flux obligatoriu:

1. git status
2. git branch
3. git pull --ff-only
4. git checkout feature/w096-imdn-foundation
5. git checkout -b feature/w097-is-composing
6. implementează incremental
7. build complet
8. ctest --output-on-failure
9. actualizează documentația
10. git push -u origin feature/w097-is-composing
11. fără merge în main

Raport final obligatoriu:

1. branch
2. branch de pornire
3. fișiere modificate
4. cum se generează active/idle/gone
5. debounce/rate limiting
6. configurare timere
7. cum funcționează indicatorul UI
8. modificările în export JSON
9. ce rămâne neimplementat
10. teste rulate
11. limitări
12. commituri
13. git status
14. push status
