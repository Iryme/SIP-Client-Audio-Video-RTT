# Task-W099 — XCAP Foundation (agent prompt, verbatim)

Repo: https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Branch: `feature/w099-xcap-foundation`
Pornește din: `feature/w098-presence-foundation`

## Obiectiv

Implementează infrastructura XCAP pentru interoperabilitate cu SIP-Server-RTT
și alte implementări SIP SIMPLE. În această etapă se implementează clientul
XCAP și diagnosticul asociat. Nu implementa încă Resource Lists avansate sau
politici complexe.

## Reguli permanente

- Nu hardcoda IP-uri, domenii, porturi, utilizatori, parole sau URI-uri.
- Toate endpoint-urile XCAP trebuie configurate din profil/config/UI.
- Nu rupe audio/video/RTT, SIP MESSAGE, IMDN, is-composing sau Presence.
- Nu activa MSRP.
- Nu implementa Resource Lists complete.
- Nu bloca UI thread.
- Actualizează docs și project-status.
- Nu face merge în main/release.

## Obiectiv funcțional

Clientul trebuie să poată:
- configura un server XCAP;
- efectua GET/PUT/DELETE pentru documente XCAP;
- descărca și valida documente XML;
- afișa diagnosticele XCAP;
- exporta operațiile XCAP în JSON;
- integra operațiile în SIP Ladder/Diagnostics atunci când este relevant.

## Cerințe

1. **XCAP model** — `XcapServerConfig`, `XcapDocument`, `XcapOperation`,
   `XcapResult`, cu cel puțin: root URI, AUID, XUI, document selector, node
   selector, ETag, Last-Modified, HTTP status, Content-Type, timestamp,
   request duration, parse status, warnings.
2. **XCAP client** — client HTTP asincron folosind infrastructura Qt deja
   existentă (GET/PUT/DELETE, HEAD opțional). Nu bloca UI.
3. **Autentificare** — Basic; Digest dacă infrastructura existentă permite;
   nu implementa OAuth; nu salva parole în clar; folosește mecanismul
   existent al profilurilor.
4. **XML validation** — validează XML înainte de PUT cu `QXmlStreamReader`;
   respinge XML invalid, document gol când nu este permis, encoding
   necunoscut; nu permite DTD externe.
5. **AUID suportate** — pregătește infrastructura pentru pres-rules,
   resource-lists, rls-services, xcap-caps, tratate ca documente XML generice
   în această etapă (fără logică specifică fiecărui document).
6. **Diagnostics** — pagină "XCAP Diagnostics" cu metodă, URL (redacted),
   HTTP status, Content-Type, Content-Length, request duration, ETag,
   Last-Modified, rezultat validare XML, warnings; fără credențiale afișate.
7. **URL redaction** — redactează userinfo, token-uri, query sensibil;
   păstrează schemă, host, port, AUID, selector parțial.
8. **Export JSON** — extinde exportul interop cu `xcapEvents` (method,
   timestamp, duration, urlRedacted, auid, xui, selector, status, etag,
   lastModified, parseStatus, warnings); păstrează `schemaVersion` 2 dacă
   modificarea este doar aditivă.
9. **Config** — `enableXcap`, `xcapRoot`, `xcapXui`, `xcapUsername`,
   `xcapAuthentication`, `validateXmlBeforePut`, `xcapTimeout`,
   `xcapVerifyTls`; implicit disabled.
10. **UI** — pagină separată "XCAP" cu configurare server, Test Connection,
    GET/PUT/DELETE, vizualizare document, rezultat operație, jurnal
    operații; nu înghesui în Call Control sau Presence.
11. **Ladder / Diagnostics** — operațiile HTTP nu trebuie desenate ca trafic
    SIP; adaugă referințe în Diagnostics și Export.
12. **Tests** — URL parsing, selector parsing, XML validation, GET/PUT/DELETE
    request model, Basic auth, Digest auth (dacă implementat), URL
    redaction, export JSON, invalid XML, timeout, regresii W090–W098.
13. **Test manual** — documentează scenariu: configurare XCAP, GET
    xcap-caps, GET resource-lists, PUT document, DELETE document, verificare
    export JSON, folosind placeholdere `<xcap-root>`, `<xui>`, `<user>`.
14. **Docs** — `docs/xcap.md` (nou); actualizează `docs/project-status.md`,
    `docs/windows-trace-json-export.md`, `docs/presence.md`,
    `docs/agent-prompts/W099-xcap-foundation.md`,
    `docs/agent-results/W099-xcap-foundation-result.md`.
15. **Versioning** — respectă politica proiectului (actualizează doar
    documentația de versiune folosită de proiect); nu crea release; nu face
    merge.

## Commituri recomandate

- `feat(xcap): add xcap client models`
- `feat(xcap): implement asynchronous xcap operations`
- `feat(xcap): add xcap diagnostics`
- `feat(ui): add xcap configuration page`
- `feat(export): include xcap diagnostics`
- `test(xcap): add xcap validation tests`
- `docs(xcap): document xcap workflow`

## Flux obligatoriu

1. `git status`
2. `git branch`
3. `git log --oneline -12`
4. `git checkout feature/w098-presence-foundation`
5. `git pull --ff-only`
6. `git checkout -b feature/w099-xcap-foundation`
7. implementează incremental
8. build complet `ENABLE_PJSIP=ON`
9. `ctest --output-on-failure`
10. actualizează documentația
11. `git push -u origin feature/w099-xcap-foundation`
12. fără merge în main

## Raport final obligatoriu

1. branch
2. branch de pornire
3. fișiere modificate/adăugate
4. modele XCAP introduse
5. operații HTTP implementate
6. autentificare suportată
7. validare XML
8. ce AUID-uri sunt pregătite
9. cum funcționează XCAP Diagnostics
10. modificările exportului JSON
11. opțiuni configurabile
12. ce NU este implementat
13. teste automate rulate
14. test manual documentat
15. limitări
16. commituri
17. git status
18. push status
