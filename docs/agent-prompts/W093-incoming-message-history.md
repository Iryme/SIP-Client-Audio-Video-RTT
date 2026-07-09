# Agent Prompt — Task W093: Incoming MESSAGE Handling + Message History

Verbatim task prompt as received, preserved for provenance.

---

Task-W093 — Incoming MESSAGE Handling + Message History

Branch:
feature/w093-incoming-message-history

Pleacă din:
feature/w092-sip-message-foundation

Obiectiv:
Adaugă callback dedicat pentru SIP MESSAGE primit și istoric messaging mai clar, fără să implementezi încă IMDN automat, Presence, XCAP sau MSRP real.

Cerințe:
1. Adaugă callback dedicat PJSIP/pjsua2 pentru incoming instant message:
   - From
   - To
   - Contact, dacă există
   - Content-Type
   - body
   - timestamp
   - account/profile asociat, dacă se poate determina.

2. Nu dubla parsing-ul:
   - mesajul primit trebuie mapat în MessagingEventStore;
   - păstrează pipeline-ul W090/W091 pentru SIP Ladder/raw diagnostics;
   - evită duplicate evidente între trace-capture și callback.

3. Istoric Messaging:
   - afișează inbound/outbound într-o listă conversațională simplă;
   - timestamp;
   - peer URI;
   - content-type;
   - body preview;
   - status outbound: queued/sent/failed/unknown;
   - status inbound: received.

4. Adaugă filtrare minimă:
   - All
   - Inbound
   - Outbound
   - Failed
   - Content-Type.

5. Confirmare status outbound:
   - dacă PJSIP permite callback/error pentru sendInstantMessage, folosește-l;
   - dacă nu, marchează explicit ca submitted, nu delivered.

6. Safety:
   - nu bloca UI thread;
   - toate update-urile UI prin signal/slot queued;
   - body preview limitat;
   - body raw mare să nu blocheze UI.

7. Tests:
   - incoming MESSAGE callback mapping;
   - dedup logic;
   - history append inbound;
   - history append outbound;
   - failed outbound status;
   - filtering;
   - preview limit;
   - UTF-8 body.

8. Docs:
   - docs/message-history.md
   - update docs/sip-message.md
   - update docs/messaging-event-store.md
   - update docs/project-status.md
   - docs/agent-prompts/W093-incoming-message-history.md
   - docs/agent-results/W093-incoming-message-history-result.md

Commituri recomandate:
- feat(messaging): add incoming message callback mapping
- feat(messaging): add message history model
- feat(ui): add message history filters
- test(messaging): add incoming history tests
- docs(messaging): document incoming message history

Raport final obligatoriu:
1. branch
2. branch de pornire
3. fișiere modificate
4. cum sunt primite MESSAGE-urile
5. cum se evită duplicatele
6. ce afișează istoricul
7. ce statusuri outbound există
8. ce NU este încă implementat
9. ce este încă diagnostic-only
10. teste rulate
11. limitări
12. commituri
13. git status
14. push status
