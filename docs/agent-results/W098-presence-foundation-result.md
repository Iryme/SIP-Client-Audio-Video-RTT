# Task W098 — Presence Foundation — Result

1. **Branch**: `feature/w098-presence-foundation`

2. **Branch de pornire**: `feature/w097-is-composing`

3. **Versiune veche/nouă și unde este urmărită**: versiunea aplicației este
   urmărită în `CMakeLists.txt` (`project(SIPClient VERSION 0.1.0 ...)`,
   configurată în `generated/AppVersion.h` la build). Rămâne `0.1.0` — nu a
   fost incrementată de niciunul dintre task-urile W090–W097, iar
   convenția stabilită în acest repo este să se actualizeze doar
   `docs/project-status.md` (counter de task-uri: 51 → 52) și documentul
   dedicat funcționalității (`docs/presence.md`), nu `CMakeLists.txt`. Nu s-a
   creat niciun release/tag.

4. **Fișiere modificate/adăugate**:
   - Noi: `src/sip/PresenceInfo.h`, `src/sip/PidfParser.h`/`.cpp`,
     `src/sip/PresenceResubscribePolicy.h`, `src/sip/PresenceStore.h`/`.cpp`,
     `src/sip/PresenceTraceEntry.h`, `src/sip/PresenceDiagnosticsStore.h`/`.cpp`,
     `src/gui/panels/PresencePage.h`/`.cpp`, `docs/presence.md`,
     `docs/agent-prompts/W098-presence-foundation.md`,
     `docs/agent-results/W098-presence-foundation-result.md`,
     `tests/test_pidf_parser.cpp`, `tests/test_presence_store.cpp`,
     `tests/test_presence_diagnostics_store.cpp`,
     `tests/test_presence_resubscribe_policy.cpp`.
   - Modificate: `src/sip/SipAccount.h`/`.cpp` (Buddy-based subscribe/
     unsubscribe/refresh/own-status API + `buddyPresenceChanged` signal),
     `src/sip/SipManager.h`/`.cpp` (public subscribe/unsubscribe/refresh/
     setOwnPresenceState, `onAccountBuddyPresenceChanged`, auto-resubscribe
     backoff), `src/core/AppSettings.h` (presence/* settings),
     `src/sip/InteropTraceExporter.h`/`.cpp` (`presenceEvents` array),
     `src/gui/SipLadderWidget.cpp` (PIDF tag, SUBSCRIBE/NOTIFY colors),
     `src/gui/MainWindow.h`/`.cpp` (new "Presence" nav page, index 4,
     `kPageCount` 8→9), `src/gui/panels/NavRail.cpp` (new nav button),
     `CMakeLists.txt`, `tests/CMakeLists.txt`,
     `tests/test_windows_trace_json_export.cpp` (presenceEvents tests),
     `docs/sip-message.md`, `docs/windows-trace-json-export.md`,
     `docs/project-status.md`.

5. **Ce parsează PIDF**: `PidfParser::parse()` (`src/sip/PidfParser.cpp`) —
   `QXmlStreamReader`, namespace-agnostic (matches elements by local name
   only), tolerant of element order/unknown fields. Extracts
   `presence@entity`, the first `<tuple>@id`, `status/basic`, `contact` +
   its `priority` attribute, `note`, `timestamp`. Recognizes RPID-style
   activity element names (`away`/`busy`/`on-the-phone`/`do-not-disturb`/
   `offline`) anywhere in the document as an extended-status hint — not a
   full RPID/CIPID implementation. Rejects bodies over 64 KiB
   (`kMaxPidfBytes`) before ever parsing them; never resolves external
   entities/DTDs (inherent to `QXmlStreamReader`); marks `Partial` for a
   missing entity/incomplete tuple, `Error` for empty/oversized/malformed
   XML, `Ok` otherwise.

6. **Cum funcționează SUBSCRIBE**: `SipManager::subscribePresence(targetUri,
   error)` — gated by `AppSettings::enablePresence()` **and**
   `enablePresenceSubscribe()` (ambele OFF implicit). Deleagă la
   `SipAccount::subscribePresence()`, care creează un `pj::Buddy` cu
   `subscribe=true`, deținut pe termen lung de `SipAccount::Impl` (pjsua2
   cere ca instanța originală să rămână vie cât ține subscripția — vezi
   `pjsua2/presence.hpp`). `Event: presence`/`Accept: application/pidf+xml`
   sunt construite intern de pjsua2. **Limitare**: `pjsua2::BuddyConfig` nu
   are câmp `expires` — valoarea `presenceDefaultExpiresSeconds` (implicit
   300s) e afișată în UI dar nu ajunge încă pe antetul `Expires` al
   SUBSCRIBE-ului trimis (nu s-a construit manual o tranzacție SIP pentru a
   ocoli această limitare, conform regulii task-ului).

7. **Cum funcționează NOTIFY**: nu există un callback pjsua2 dedicat separat
   de `Buddy::onBuddyState()` (stare) — pentru inspecția completă a unui
   NOTIFY (Event/Subscription-State/Expires/reason/Content-Type/body/
   Call-ID/CSeq/From/To/timestamp) este folosit **pipeline-ul de
   diagnostic**: `PresenceDiagnosticsStore` se abonează la
   `SipTraceLogger` (aceeași captură brută folosită de SIP Ladder) și
   extrage headerele + parsează body-ul `application/pidf+xml` prin
   `PidfParser`, construind un `PresenceTraceEntry`. Un NOTIFY fără body
   (valid conform RFC 3265, ex. NOTIFY final la terminare) nu este tratat
   ca eroare de parsare.

8. **Lifecycle și auto-resubscribe**: stări pending/active/terminated/
   unknown, mapate din `pjsip_evsub_state` în `SipAccount`'s
   `onBuddyState()`. La `terminated`, `SipManager::
   onAccountBuddyPresenceChanged()` consultă `PresenceResubscribePolicy`
   (logică pură, fără PJSIP, testată separat): `shouldAutoRetry(reason)`
   întoarce `false` pentru `rejected`/`noresource` (nu se reîncearcă
   automat), `true` pentru celelalte motive normalizate
   (timeout/deactivated/probation/giveup/invariant/unknown).
   `backoffMs(attempt)` = backoff exponențial, bază 5s, dublare per
   încercare, plafonat la 300s — nu se creează niciodată o buclă rapidă de
   SUBSCRIBE. Auto-resubscribe rulează doar dacă Presence + Subscribe +
   `presenceAutoResubscribe` (implicit ON, dar irelevant până Presence e
   activ) sunt toate active. Un "Unsubscribe" explicit din UI anulează orice
   temporizator de backoff în așteptare pentru acea entitate.

9. **Cum funcționează PresenceStore**: `src/sip/PresenceStore.h/.cpp` —
   singleton, `QHash<QString, PresenceInfo>` cu starea curentă per
   `entityUri` (upsert), plus un istoric mărginit (`QList<PresenceInfo>`,
   `presenceMaxRetainedEvents`, implicit 500, cele mai vechi evacuate
   primele). Protejat cu mutex; semnale Qt `presenceUpdated`/`cleared`.
   Alimentat **exclusiv** de pipeline-ul de subscripție live (`SipAccount`
   → `SipManager`) — niciodată de `PresenceDiagnosticsStore`, pentru a evita
   doi scriitori concurenți pe aceeași stare. Complet separat de
   `MessageHistoryStore`.

10. **Ce afișează UI-ul**: o pagină nouă de navigare "Presence"
    (`src/gui/panels/PresencePage.h/.cpp`), separată de Call control și
    Messaging Diagnostics: toggle-uri (Enable Presence/Subscribe/Publish
    experimental/Auto-resubscribe), câmp target SIP URI + Expires
    configurabil + butoane Subscribe/Unsubscribe/Refresh, selector de stare
    proprie (Available/Away/Busy/Do Not Disturb/Offline) + buton "Set" clar
    marcat experimental, și un tabel (entity, basic state, extended state,
    note, subscription state, expires, last update, status/error) alimentat
    live din `PresenceStore`.

11. **PUBLISH — implementat sau experimental**: **experimental**.
    `SipAccount::setOwnPresenceState()` apelează întotdeauna
    `Account::setOnlineStatus()` (actualizează starea locală folosită la
    răspunsul către watcher-ii proprii), dar trimiterea efectivă a unui
    PUBLISH necesită `AccountConfig.presConfig.publishEnabled = true`, pe
    care pjsua2 îl citește **doar la crearea contului** — bifarea "Enable
    Publish" nu are efect până la o re-înregistrare a profilului activ.
    Documentat explicit în UI și în `docs/presence.md`. Nu s-a construit
    manual nicio tranzacție PUBLISH brută.

12. **Integrarea în SIP Ladder**: `contentTypeTag()` etichetează
    `application/pidf+xml` cu `[PIDF]`; `colorForTrace()` are culori
    dedicate pentru SUBSCRIBE (`#4FC3E8`) și NOTIFY (`#4FE8B0`). Nicio altă
    modificare nu a fost necesară — captura brută (`PjsipTraceModule`) și
    redactarea (`Authorization`/`Proxy-Authorization` → `[REDACTED]`) sunt
    deja agnostice la metodă.

13. **Modificările în exportul JSON**: `InteropTraceExporter::
    exportToJson()` are acum un array nou la nivel de rădăcină,
    `presenceEvents`, complet independent de `events` — `eventId`,
    `timestamp`, `direction`, `method`, `callId`, `cseq`, `from`, `to`,
    `eventPackage`, `subscriptionState`, `subscriptionExpires`,
    `subscriptionReason`, `contentType`, `parseStatus`, `parseWarnings`,
    `rawSipRedacted`, plus un obiect imbricat `presence` (entity, tupleId,
    basicStatus, extendedStatus, contact, priority, note, timestamp). Este
    o adăugare pur aditivă (o cheie nouă la rădăcină) — `kSchemaVersion`
    rămâne **2**.

14. **Ce este configurabil** (`AppSettings`, prefix `presence/`):
    `enablePresence` (OFF), `enablePresenceSubscribe` (OFF),
    `enablePresencePublish` (OFF, experimental), `presenceDefaultExpiresSeconds`
    (300), `presenceAutoResubscribe` (ON), `presenceMaxRetainedEvents`
    (500), `presenceDefaultState` ("available").

15. **Ce NU este implementat**:
    - XCAP, resource lists — explicit în afara scopului (Task W099).
    - Suport complet RPID/CIPID (doar recunoaștere de nume de elemente
      pentru away/busy/on-the-phone/do-not-disturb/offline).
    - Valoarea `Expires` configurată nu ajunge încă pe SUBSCRIBE-ul trimis
      (limitare API pjsua2 — vezi punctul 6).
    - Publish complet dinamic (necesită re-înregistrare — punctul 11).
    - Integrare cu ContactsPanel (tabelul de Presence e independent).
    - MSRP rămâne complet dezactivat și neatins.

16. **Teste automate rulate**: build complet cu `ENABLE_PJSIP=ON` (`nmake`,
    curat, zero erori) urmat de `ctest --output-on-failure`: **100% tests
    passed, 0 failed, 51 total** (47 anterioare + 4 noi:
    `test_pidf_parser`, `test_presence_store`,
    `test_presence_diagnostics_store`, `test_presence_resubscribe_policy`).
    Testele acoperă: PIDF open/closed/contact+priority/note/namespace/tuple
    multiple/incomplet/XML invalid/body gol/body prea mare, extended status
    away/busy/do-not-disturb, model cerere SUBSCRIBE, NOTIFY active/pending/
    terminated (motive rejected/timeout), parsare expires (din
    `Subscription-State` și din headerul `Expires`), NOTIFY fără body,
    redactare SIP brut, NOTIFY duplicat, actualizare store per entitate,
    actualizare "stale" (last-write-wins, documentat), politica de
    backoff/no-retry pentru rejected/noresource, export JSON
    `presenceEvents` (schema rămâne v2), regresii W090–W097 (toate cele 47
    de teste anterioare trec neschimbate).

17. **Test manual cu serverul**: documentat pas-cu-pas în
    `docs/presence.md` §15 (placeholders `sip:<user>@<server-host>`,
    `<profile-name>`, fără IP-uri/utilizatori reali). **Nu a fost executat**
    în această sesiune — niciun SIP-Server-RTT live nu a fost disponibil.

18. **Limitări**: vezi punctele 6, 11, 15 de mai sus; în plus,
    `PresenceDiagnosticsStore` și pipeline-ul live (`SipAccount`/pjsua2
    Buddy) sunt intenționat independente (niciun test nu poate valida
    interacțiunea reală SUBSCRIBE→NOTIFY cu un server fără o instanță
    PJSIP live — motiv pentru care logica de mapare/politică a fost
    separată în funcții pure testabile fără rețea, conform cerinței 15 a
    task-ului).

19. **Commituri**: listate mai jos, pe `feature/w098-presence-foundation`
    (a se vedea `git log` pentru hash-urile exacte după commit):
    - `feat(presence): add presence model and pidf parser`
    - `feat(presence): implement subscribe and notify lifecycle`
    - `feat(presence): add presence store`
    - `feat(ui): add presence page and subscription controls`
    - `feat(export): include presence diagnostics`
    - `test(presence): add pidf and subscription tests`
    - `docs(presence): document presence workflow`

20. **git status**: curat după commit-urile de mai sus (confirmat înainte de
    push).

21. **Push status**: `git push -u origin feature/w098-presence-foundation`.
    Fără merge în main/release.
