# Agent Prompt — Task W091: Messaging Event Model + Safe Message Store

Verbatim task prompt as received, preserved for provenance.

---

Task-W091 — Messaging Event Model + Safe Message Store

Branch:
feature/w091-messaging-event-store

Pleacă din:
release/v1.4.0 sau din feature/w090-msrp-lmpe-diagnostics dacă W090 nu este încă integrat în release.

Obiectiv:
Introdu un model intern unificat pentru evenimente de messaging, independent de transport, astfel încât UI-ul și exportul să nu depindă direct de SIP MESSAGE sau MSRP.

Reguli:
- Nu hardcoda IP-uri, domenii, porturi, useri, parole sau URI-uri.
- Nu rupe audio/video/RTT existent.
- Nu activa MSRP real.
- Nu trimite încă SIP MESSAGE.
- Totul rămâne diagnostic/read-only.
- Actualizează docs și project-status.
- Nu face merge în main.

Cerințe:

1. Adaugă MessagingEvent model:
   - id intern unic
   - timestamp
   - direction: inbound/outbound/unknown
   - transport: sip-message/msrp/unknown
   - payloadType: plain/html/cpim/imdn/is-composing/sdp/unknown
   - from
   - to
   - callId
   - cseq
   - contentType
   - bodyPreview
   - rawSipRedacted
   - parseStatus: ok/partial/error
   - parseWarnings list

2. Adaugă MessagingEventStore:
   - append event
   - clear
   - count
   - get snapshot/list
   - export JSON
   - export TXT
   - thread-safe sau explicit protejat pentru UI thread
   - limită configurabilă de evenimente păstrate în memorie

3. Integrează W090:
   - rezultatele CPIM/IMDN/is-composing/SDP MSRP trebuie mapate în MessagingEvent
   - păstrează compatibilitate cu panelul Messaging Diagnostics existent
   - nu duplica logica de parsing în UI

4. UI:
   - Messaging Diagnostics trebuie să citească din MessagingEventStore
   - adaugă buton Clear
   - adaugă buton Export JSON
   - adaugă buton Export TXT
   - afișează parse warnings fără să blochezi UI

5. Config:
   - adaugă opțiune pentru max events retained
   - default rezonabil, de exemplu 1000
   - să poată fi schimbat din config/UI dacă există mecanism

6. Tests:
   - test append/clear/count
   - test export JSON
   - test export TXT
   - test limită max events
   - test mapare SIP MESSAGE -> MessagingEvent
   - test parse warning pentru content invalid/parțial

7. Docs:
   - docs/messaging-event-store.md
   - update docs/messaging-diagnostics.md
   - update docs/project-status.md
   - docs/agent-prompts/W091-messaging-event-store.md
   - docs/agent-results/W091-messaging-event-store-result.md

Commituri recomandate:
- feat(messaging): add unified messaging event model
- feat(messaging): add safe messaging event store
- feat(messaging): map diagnostics into messaging events
- feat(ui): wire messaging diagnostics to event store
- test(messaging): add messaging event store tests
- docs(messaging): document event store workflow

Raport final obligatoriu:
1. branch
2. fișiere modificate
3. modelul MessagingEvent introdus
4. cum se mapează W090 în noul store
5. ce rămâne diagnostic-only
6. ce NU este încă implementat
7. teste rulate
8. limitări
9. commituri
10. git status
11. push status
