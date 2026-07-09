# Agent Prompt — Task W094: Windows Messaging/MSRP Diagnostics JSON Export

Verbatim task prompt as received, preserved for provenance.

---

Task-W094 — Windows Messaging/MSRP Diagnostics JSON Export

Branch:
feature/w094-windows-trace-json-export

Pleacă din:
feature/w093-incoming-message-history

Obiectiv:
Adaugă export JSON compatibil cu serverul SIP-Server-RTT și cu scriptul:
scripts/interop/compare-client-server-trace.py

Scop:
Clientul Windows trebuie să poată exporta local evenimente de messaging/MSRP diagnostics într-un format comun cu serverul, pentru comparație client-server.

Cerințe:

1. Export JSON unificat
   Exportă evenimente pentru:
   - SIP MESSAGE
   - CPIM
   - IMDN
   - is-composing
   - SDP MSRP diagnostics

2. Compatibilitate câmpuri server
   Include câmpuri comune:
   - schemaVersion
   - source: windows-client
   - exportedAt
   - eventId
   - timestamp
   - direction
   - status
   - transport
   - payloadType
   - Call-ID
   - CSeq
   - From
   - To
   - Content-Type
   - bodyPreview
   - rawSipRedacted

3. Messaging fields
   Include unde există:
   - Message-ID
   - IMDN ID
   - original-recipient
   - final-recipient
   - IMDN disposition: delivered/displayed/failed/error
   - CPIM From
   - CPIM To
   - CPIM DateTime
   - CPIM Subject
   - CPIM Content-Type
   - is-composing state
   - is-composing timeout
   - is-composing refresh

4. MSRP/SDP fields
   Include unde există:
   - session_id
   - transaction_id, dacă există
   - From-Path
   - To-Path
   - m=message
   - a=path
   - a=accept-types
   - a=setup
   - a=connection
   - transport protocol:
     - TCP/MSRP
     - TCP/TLS/MSRP

5. Nu activa MSRP real
   - exportul rămâne diagnostic-only;
   - nu deschide socket MSRP;
   - nu negocia sesiuni MSRP;
   - nu modifica call/media behavior.

6. UI
   În Messaging Diagnostics adaugă:
   - Export Server-Compatible JSON
   sau redenumește exportul existent clar:
   - Export JSON
   - Export Interop JSON

   Nu înghesui call control.

7. Script compatibility
   Verifică formatul așteptat de:
   scripts/interop/compare-client-server-trace.py

   Dacă repo-ul client nu conține scriptul, documentează câmpurile comune pe baza cerinței și creează sample export.

8. Tests
   Adaugă teste pentru:
   - export SIP MESSAGE basic
   - export CPIM fields
   - export IMDN fields
   - export is-composing fields
   - export SDP MSRP fields
   - raw SIP redacted
   - sample export valid JSON
   - câmpuri obligatorii prezente

9. Docs
   Adaugă/actualizează:
   - docs/windows-trace-json-export.md
   - docs/messaging-diagnostics.md
   - docs/msrp-diagnostics.md
   - docs/project-status.md
   - docs/agent-prompts/W094-windows-trace-json-export.md
   - docs/agent-results/W094-windows-trace-json-export-result.md

10. Exemplu documentat
   Include în docs un sample minimal:

   {
     "schemaVersion": 1,
     "source": "windows-client",
     "exportedAt": "...",
     "events": [
       {
         "timestamp": "...",
         "direction": "inbound",
         "transport": "sip-message",
         "payloadType": "cpim",
         "callId": "...",
         "messageId": "...",
         "contentType": "message/cpim",
         "cpim": {
           "from": "...",
           "to": "...",
           "dateTime": "...",
           "subject": "...",
           "contentType": "text/plain"
         },
         "rawSipRedacted": "..."
       }
     ]
   }

Commituri recomandate:
- feat(export): add interop trace json schema
- feat(export): map messaging diagnostics to interop json
- feat(export): include msrp diagnostic fields
- feat(ui): add interop json export button
- test(export): add windows trace json export tests
- docs(export): document interop trace export

Raport final obligatoriu:
1. branch
2. branch de pornire
3. fișiere modificate
4. format JSON introdus
5. câmpuri comune cu serverul
6. câmpuri CPIM/IMDN/is-composing exportate
7. câmpuri MSRP/SDP exportate
8. compatibilitate cu compare-client-server-trace.py
9. ce rămâne diagnostic-only
10. ce NU este implementat
11. teste rulate
12. limitări
13. commituri
14. git status
15. push status
