# Agent Prompt — Fix: Decode Content-Encoding deflate for IMDN and messaging diagnostics

Verbatim task prompt as received, preserved for provenance.

---

Task-W092 — Decode Content-Encoding deflate for IMDN and messaging diagnostics

Context:
În exportul Windows client, Linphone trimite unele IMDN-uri astfel:

Content-Type: message/imdn+xml
Content-Encoding: deflate

Clientul marchează acum parseStatus=partial, deoarece încearcă să parseze body-ul comprimat ca XML brut.

Obiectiv:
Adaugă suport pentru Content-Encoding: deflate înainte de parsarea IMDN / is-composing / CPIM body.

Cerințe:
1. Detectează headerul:
   Content-Encoding: deflate
2. Decomprimă body-ul înainte de XML parsing.
3. Păstrează rawSipRedacted neschimbat.
4. bodyPreview trebuie să afișeze conținutul decodat.
5. Export JSON trebuie să includă:
   - contentEncoding
   - decodedBodyPreview
   - parseStatus
   - parseWarnings
6. Dacă decompression eșuează:
   - parseStatus=partial
   - warning clar: "deflate decode failed"
7. Nu rupe text/plain, CPIM, IMDN plain XML, is-composing.
8. Adaugă teste:
   - IMDN plain XML
   - IMDN deflate valid
   - IMDN deflate invalid
   - is-composing plain
9. Actualizează docs:
   - docs/messaging-diagnostics.md
   - docs/agent-prompts/W092-imdn-deflate-decoding.md
   - docs/agent-results/W092-imdn-deflate-decoding-result.md
   - docs/project-status.md

Branch:
fix/w092-imdn-deflate-decoding

Commituri recomandate:
- fix(messaging): decode deflate encoded imdn payloads
- test(messaging): cover deflate encoded diagnostics
- docs(messaging): document encoded payload handling

Raport final:
1. branch
2. fișiere modificate
3. ce encoding-uri suportă
4. teste rulate
5. limitări
6. commituri
7. git status
8. push status
