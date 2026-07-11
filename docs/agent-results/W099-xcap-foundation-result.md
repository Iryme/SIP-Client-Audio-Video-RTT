# Task W099 — XCAP Foundation — Result

1. **Branch**: `feature/w099-xcap-foundation`

2. **Branch de pornire**: `feature/w098-presence-foundation`

3. **Versiune veche/nouă și unde este urmărită**: versiunea aplicației este
   urmărită în `CMakeLists.txt` (`project(SIPClient VERSION 0.1.0 ...)`).
   Rămâne `0.1.0` — nu a fost incrementată de niciunul dintre task-urile
   W090–W099; convenția stabilită în acest repo este să se actualizeze doar
   `docs/project-status.md` (counter de task-uri: 52 → 53) și documentul
   dedicat funcționalității (`docs/xcap.md`). Nu s-a creat niciun
   release/tag.

4. **Fișiere modificate/adăugate**:
   - Noi: `src/sip/XcapModels.h`, `src/sip/XcapUrlRedactor.h`/`.cpp`,
     `src/sip/XcapXmlValidator.h`/`.cpp`, `src/sip/XcapRequestBuilder.h`/`.cpp`,
     `src/sip/XcapAuthHeaderBuilder.h`, `src/sip/XcapClient.h`/`.cpp`,
     `src/sip/XcapDiagnosticsStore.h`/`.cpp`, `src/gui/panels/XcapPage.h`/`.cpp`,
     `docs/xcap.md`, `docs/agent-prompts/W099-xcap-foundation.md`,
     `docs/agent-results/W099-xcap-foundation-result.md`,
     `tests/test_xcap_url_redactor.cpp`, `tests/test_xcap_xml_validator.cpp`,
     `tests/test_xcap_request_builder.cpp`, `tests/test_xcap_auth_header_builder.cpp`,
     `tests/test_xcap_diagnostics_store.cpp`.
   - Modificate: `src/core/AppSettings.h` (`xcap/*` settings),
     `src/sip/InteropTraceExporter.h`/`.cpp` (`xcapEvents` array),
     `src/gui/panels/NavRail.cpp` (new "XCAP" nav button),
     `src/gui/MainWindow.h`/`.cpp` (new "XCAP" nav page, index 5,
     `kPageCount` 9→10, all later indices renumbered), `CMakeLists.txt`,
     `tests/CMakeLists.txt`, `docs/project-status.md`,
     `docs/windows-trace-json-export.md`, `docs/presence.md`.

5. **Modele XCAP introduse** (`src/sip/XcapModels.h`): `XcapServerConfig`
   (rootUri, xui, username, authMode, validateXmlBeforePut, timeoutSeconds,
   verifyTls — parola nu e niciodată câmp aici), `XcapDocument` (auid, xui
   opțional per-document, documentName, nodeSelector, `buildUri()`),
   `XcapOperation` (cererea înainte de execuție), `XcapResult` (singurul tip
   la care UI-ul se leagă: method, rootUri, auid, xui, documentSelector,
   nodeSelector, urlRedacted, httpStatus/httpReason, contentType,
   contentLength, etag, lastModified, timestamp, durationMs, parseStatus,
   warnings, bodyPreview, networkError, errorString).

6. **Operații HTTP implementate**: GET, PUT, DELETE, HEAD — toate asincron
   prin `QNetworkAccessManager` (`XcapClient`), niciodată blocante pentru UI.
   `XcapRequestBuilder::build()` (funcție pură, testată fără rețea) separă
   construcția modelului de cerere (URL, metodă, Content-Type, timeout) de
   execuția efectivă din `XcapClient::execute()`. PUT validează XML-ul
   sincron înainte de a deschide vreo conexiune, când
   `validateXmlBeforePut` e activ (implicit da).

7. **Autentificare suportată**: None (implicit), Basic (header
   `Authorization: Basic ...` construit proactiv, fără round-trip de
   descoperire — `XcapAuthHeaderBuilder::basicAuthorizationHeader()`,
   testat izolat), Digest (delegat integral mecanismului
   `QNetworkAccessManager::authenticationRequired` din Qt — nu există cod de
   hash digest scris manual în acest client). OAuth nu este implementat
   (explicit în afara scopului). Parola nu este niciodată salvată în
   `AppSettings`/INI și nu e logată — trece prin `CredentialStore` (același
   mecanism securizat folosit de parolele profilurilor SIP), sub un
   pseudo-profil fix `"xcap"` + username-ul configurat.

8. **Validare XML**: `XcapXmlValidator::validate()` — `QXmlStreamReader`,
   fără DOM. Respinge: document gol (dacă nu e explicit permis), orice
   declarație `<!DOCTYPE` (verificat înainte de a preda documentul
   parserului — apărare suplimentară față de expansiunea de entități
   externe, pe lângă faptul că `QXmlStreamReader` nu rezolvă oricum
   DTD-uri/entități externe), un `encoding=` necunoscut (listă albă:
   utf-8/utf-16/us-ascii/iso-8859-1), erori de sintaxă, element rădăcină
   lipsă/dezechilibrat. Aceeași funcție clasifică și răspunsurile GET
   (`parseStatus`/`warnings` în `XcapResult`) când `Content-Type` conține
   "xml".

9. **AUID-uri pregătite**: combo box pre-populat cu `resource-lists`,
   `pres-rules`, `rls-services`, `xcap-caps` (editabil liber pentru orice
   alt AUID) — dar **fără nicio logică/schemă specifică per AUID**; fiecare
   document este tratat identic ca XML generic, exact conform cerinței 5.

10. **Cum funcționează XCAP Diagnostics**: un jurnal de operații
    (`XcapDiagnosticsStore`, mărginit la 500 intrări, cel mai vechi
    eliminat primul) afișat în pagina "XCAP" — coloane Method, URL
    (redacted), HTTP Status, Content-Type, Duration, ETag, Last-Modified,
    Parse status, Timestamp; niciodată credențiale afișate (nici măcar un
    câmp pentru ele în `XcapResult`). Operațiile HTTP nu apar niciodată în
    SIP Ladder — este HTTP simplu, nu trafic SIP.

11. **Modificările exportului JSON**: `InteropTraceExporter::exportToJson()`
    are acum un nou array `xcapEvents` la nivel de rădăcină, complet
    independent de `events`/`presenceEvents` (eventId, method, timestamp,
    duration, urlRedacted, auid, xui, selector, nodeSelector, status,
    contentType, contentLength, etag, lastModified, parseStatus, warnings,
    networkError, errorString). Este pur aditiv — `schemaVersion` rămâne
    **2**.

12. **Opțiuni configurabile** (`AppSettings`, prefix `xcap/`): `enableXcap`
    (implicit false), `xcapRoot` (implicit gol), `xcapXui` (implicit gol),
    `xcapUsername` (implicit gol), `xcapAuthentication` (implicit
    `"none"`), `validateXmlBeforePut` (implicit true), `xcapTimeout`
    (implicit 15s), `xcapVerifyTls` (implicit true). Totul dezactivat
    implicit, niciun URI implicit.

13. **Ce NU este implementat**: semantica Resource Lists (`<list>`/`<entry>`),
    semantica pres-rules, semantica rls-services (toate tratate ca XML
    generic); OAuth; PUT condiționat (`If-Match`/ETag-based concurrency);
    XCAP Diff/auto-refresh/notificări de schimbare; implementare proprie a
    algoritmului digest (se bazează integral pe Qt Network); integrare
    Presence-specifică (pres-rules) cu funcționalitatea din Task W098; MSRP
    rămâne complet neatins/dezactivat.

14. **Teste automate rulate**: build complet `ENABLE_PJSIP=ON` (0 erori) +
    `ctest --output-on-failure` → **100% tests passed, 0 failed, 56 total**
    (toate testele W090–W098 rulează neschimbate, fără regresii, plus cele
    5 ținte noi: `test_xcap_url_redactor`, `test_xcap_xml_validator`,
    `test_xcap_request_builder`, `test_xcap_auth_header_builder`,
    `test_xcap_diagnostics_store`). O eroare de link a fost întâlnită și
    corectată în timpul acestei sesiuni (vezi §16), plus un bug real găsit
    de `test_xcap_request_builder` (vezi §16).

15. **Test manual documentat**: da, în `docs/xcap.md` §13 — configurare
    server, GET `xcap-caps`, GET `resource-lists`, PUT document, verificare
    ETag, GET din nou, DELETE, export JSON — cu placeholdere
    `<xcap-root>`/`<xui>`/`<user>`. Nu a fost executat împotriva unui
    SIP-Server-RTT real în această sesiune (niciun server disponibil).

16. **Limitări**: vezi `docs/xcap.md` §12; digest auth nu are cod propriu de
    testat dincolo de faptul că este atașat doar pentru cereri configurate
    ca Digest; validarea XML este sincronă (dar rapidă, pe documente XCAP
    tipice mici) — pentru documente foarte mari ar putea introduce o
    scurtă latență pe thread-ul UI (nu există încă un plafon de dimensiune
    explicit pentru body-ul PUT, spre deosebire de PIDF's 64 KiB cap din
    Task W098).

    Două probleme au fost găsite și corectate în această sesiune, înainte
    de commit:
    - **Eroare de link**: `InteropTraceExporter.cpp`'s no-arg
      `exportToJson()` apelează acum `XcapDiagnosticsStore::instance()`, iar
      cele patru ținte de test care leagă deja `MESSAGING_DIAGNOSTICS_SOURCES`
      (`test_messaging_diagnostics_store`, `test_messaging_event_store`,
      `test_sip_message_foundation`, `test_windows_trace_json_export`) nu
      aveau `XCAP_SOURCES`/`Qt6::Network`/`CredentialStore` — corectat prin
      `list(APPEND MESSAGING_DIAGNOSTICS_SOURCES ${XCAP_SOURCES})` plus
      `SECURITY_SOURCES`/`Qt6::Network` adăugate acelor patru ținte.
    - **Bug real, găsit de `test_xcap_request_builder`**:
      `XcapDocument::buildUri()` folosea doar `xui`-ul propriu al
      documentului, ignorând `XcapServerConfig::xui` ca fallback atunci
      când documentul nu avea un XUI explicit — rezultând un URI greșit
      (`.../global/...` în loc de `.../users/<server-xui>/...`) pentru
      cazul comun în care XUI-ul e configurat o singură dată la nivel de
      server. Corectat prin adăugarea unui parametru `defaultXui` la
      `buildUri()`, actualizat în toate cele trei locuri unde era apelat
      (`XcapClient.cpp` ×2, `XcapRequestBuilder.cpp`).

17. **Commituri**:
    - `feat(xcap): add xcap client models`
    - `feat(xcap): implement asynchronous xcap operations`
    - `feat(xcap): add xcap diagnostics`
    - `feat(ui): add xcap configuration page`
    - `feat(export): include xcap diagnostics`
    - `test(xcap): add xcap validation tests`
    - `docs(xcap): document xcap workflow`

18. **git status**: working tree curat după commit, cu excepția scripturilor
    locale auxiliare `build_w098.bat`/`build_w099.bat`/`ctest_w098.bat`/
    `ctest_w099.bat` (intenționat necomise, ca și în sesiunile anterioare).

19. **Push status**: `git push -u origin feature/w099-xcap-foundation` —
    branch nou pe remote, urmărire configurată. Fără merge în
    main/release.
