# Task-W101 — Live MSRP SIP Integration — Raport Final

**1. Branch nou**: `feature/w101-live-msrp-sip-integration`

**2. Branch de pornire**: `feature/w100-msrp-foundation`

**3. Versiune veche/nouă**: `0.1.0` → `0.1.0` (neschimbată — politica proiectului nu cere bump fără release, respectată).

**4. Schema JSON veche/nouă**: `schemaVersion = 2` → **neschimbată (2)**. Faza 9 (schemaVersion 3: `transportDecision`, `msrpSession`, `peerAssociation`, `fallbackReason`, `negotiationState`) **nu a fost implementată** în această trecere — vezi punctul 24 "Limitări rămase".

**5. Fișiere modificate/adăugate**:
- Noi: `src/msrp/MsrpSipMediaInjector.{h,cpp}`, `tests/test_msrp_sip_media_injector.cpp`, `docs/msrp-live-sdp-integration.md`, `docs/msrp-peer-association.md`, `docs/msrp-fallback.md`, `docs/agent-prompts/W101-live-msrp-sip-integration.md`, acest fișier.
- Modificate: `src/sip/SipCall.h`, `src/sip/SipCall.cpp`, `src/sip/SipManager.cpp`, `src/msrp/MsrpSession.h`, `src/msrp/MsrpSession.cpp`, `src/msrp/MsrpSessionInfo.h`, `src/msrp/MsrpSipIntegration.h` (comentariu corectat), `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/msrp-foundation.md`, `docs/msrp-transport.md`, `docs/msrp-testing.md`, `docs/project-status.md`.

**6. Metoda de integrare SDP aleasă**: mutarea directă a `pjmedia_sdp_session` viu, prin `OnCallSdpCreatedParam::sdp.pjSdpSession` (pointer, nu copie), folosind exclusiv API-ul public `pjmedia/sdp.h` (`pjmedia_sdp_attr_create`, `PJ_POOL_ZALLOC_T`, alocare directă de câmpuri struct) — **niciun fișier din `.deps/pjproject` nu a fost editat**, respectând regula 1.

**7. Justificarea tehnică**: `Endpoint::on_call_sdp_created` (`.deps/pjproject/pjsip/src/pjsua2/endpoint.cpp:1547-1585`) doar compară textul `wholeSdp` înainte/după callback și re-scrie `sdp` doar dacă acel text s-a schimbat. Dacă `wholeSdp` rămâne neatins și mutăm direct `*(pjmedia_sdp_session*)pjSdpSession`, nimic nu suprascrie modificarea, iar acel exact struct este cel folosit de PJSIP pentru INVITE/answer real. Pool-ul dialogului nu este expus public din callback, deci s-a folosit `pjsua_pool_create()` (API public) — un pool per apel, eliberat în `~SipCall()`. Detalii complete: `docs/msrp-live-sdp-integration.md`.

**8. Dovada că `m=message` apare efectiv pe wire**: `tests/test_msrp_sip_media_injector.cpp` — parsează o SDP de bază (audio+video) cu `pjmedia_sdp_parse()` (exact ca PJSIP), injectează prin codul real folosit de `SipCall`, apoi re-serializează cu `pjmedia_sdp_print()` și verifică textul brut rezultat (`m=message 2855 TCP/MSRP *`, `a=path:...`, etc.) — nu modelul intern Qt. `ctest -R test_msrp_sip_media_injector` → PASS. Capturi SIP live (Wireshark/pcap) împotriva unui server real **nu au fost efectuate** — vezi punctele 19/28.

**9. Impact asupra audio/video/RTT**: zero regresii — 67/67 teste trec (66 preexistente + 1 nou), inclusiv toate testele audio/video/RTT/regression W090–W100 nemodificate. Testul de injector verifică explicit că secțiunile `m=audio`/`m=video` supraviețuiesc neatinse.

**10. Dedicated vs attached SIP dialog**: MSRP rămâne atașat la dialogul apelului audio/video/RTT existent (nu s-a implementat un dialog SIP dedicat separat) — fiecare `SipCall` are propria sa sesiune `MsrpSession`, propriul listener. Nu exista în acest task o cerință explicită de `msrpDialogMode` configurabil (spre deosebire de o versiune anterioară/mai lungă a specificației W101); alegerea "attached" este cea mai simplă, sigură, coerentă cu arhitectura W100 existentă.

**11. Mapping SIP ↔ MSRP**: `MsrpSessionInfo` are acum `sipHeaderCallId` (headerul real `Call-ID`, din `pj::CallInfo::callIdString`) și `mediaIndex` (poziția secțiunii `m=message` în SDP negociat), pe lângă `sessionKey`/`sipCallId`/`localSessionId`/`remoteSessionId`/`localPath`/`remotePath` deja existente din W100. Fiecare `SipCall` are propria `MsrpSession` cu propriul port efemer OS-assigned — mai multe sesiuni simultane către același peer nu pot coliziona structural (chei/porturi diferite per apel). Detalii: `docs/msrp-peer-association.md`.

**12. Asocierea conexiunilor MSRP**: validarea explicită a conexiunilor inbound (comparare `To-Path`/session-id la primul `SEND` primit) **nu a fost implementată** — modelul actual (un listener dedicat per sesiune, oprit după prima conexiune acceptată) previne structural coliziunile de port, dar nu validează criptografic identitatea peer-ului la nivel MSRP. Documentat explicit ca limitare în `docs/msrp-peer-association.md`.

**13. Rezolvarea diagnosticului de peer-connection**: raportul server-side original ("peer connection not found", sesiuni blocate în WAITING_FOR_CONNECTION) **nu a fost re-testat** — nu a existat acces la SIP-Server-RTT live în acest mediu. Diagnosticul dedicat de asociere (matched/no-matching-session/ambiguous/etc.) descris într-o versiune anterioară a specificației **nu face parte din acest task** și nu a fost implementat.

**14. Transport policy conectat la composer**: DA — `SipManager::sendSipMessage()` apelează acum `MessagingTransportPolicy::decideInitialTransport()`/`decideFallbackAfterMsrpFailure()` real, pentru fiecare mesaj compus, cu toate cele 4 moduri (`sip-message-only`/`msrp-preferred`/`msrp-required`/`automatic`). Niciun dual-send: dacă decizia alege MSRP, nu se compune deloc un SIP MESSAGE pentru acea încercare.

**15. Fallback SIP MESSAGE real**: DA — pe eșec MSRP (send-ul întors gol) sau indisponibilitate (`isMsrpEstablished()` fals), fallback-ul se decide prin politica pură deja testată unitar (`test_messaging_transport_policy`, nemodificat din W100), iar `msrp-required` nu face niciodată fallback. Recuperarea automată (revenire pe MSRP fără restart apel) este satisfăcută structural: decizia se recalculează proaspăt la fiecare mesaj, fără stare persistentă "am căzut pe fallback" — vezi `docs/msrp-fallback.md`.

**16. SEND → response → REPORT real**: DA pentru fluxul outbound — `SipCall::sendMsrpMessage()` apelează direct `MsrpSession::sendMessage()` pe o sesiune stabilită real. `MsrpSession::messageDeliveryStatusChanged` (semnal nou) se emite din răspunsul SEND (când nu s-a cerut REPORT, sau la eșec) sau dintr-un REPORT ulterior, fără dublă raportare. Inbound: `payloadReceived` e conectat prin `SipCall` la `SipManager`, care scrie în `MessageHistoryStore` (dedup identic cu SIP MESSAGE inbound). Corelarea `deliveryState` din `messageDeliveryStatusChanged` în `MessageHistoryStore` **nu e implementată** (spațiu de Message-ID diferit de cel SIP MESSAGE; doar logat) — vezi punctul 24.

**17. Format LMPE confirmat**: N/A — LMPE este explicit exclus din acest task ("LMPE NU se implementează în acest task").

**18-19. Export JSON / comparator client-server**: neschimbate (schemaVersion rămâne 2); niciun câmp nou de export adăugat în această trecere (Faza 9 amânată).

**20. Teste automate**: build `ENABLE_PJSIP=ON` + toate testele W090–W100 (baseline verificat curat înainte de orice modificare: 66/66) + testul nou `test_msrp_sip_media_injector` → **67/67 PASS, zero regresii**, verificat după fiecare fază de implementare.

**21. Teste manuale**: vezi punctele 25-27 mai jos.

**22-25. PASS / FAIL / BLOCKED / NOT RUN**:
- PASS: injecție SDP live (test automat), build+regresie completă, wiring transport policy → composer (verificat prin build/teste, nu prin capture live).
- FAIL: niciunul.
- BLOCKED: validare SIP-Server-RTT live (server real nu a fost disponibil în acest mediu de lucru) — capture SIP/MSRP real, comparator client-server, scenarii CPIM/IMDN/is-composing/chunked/TLS peste o sesiune reală.
- NOT RUN: Blink, AG Projects, Linphone (niciun client de referință disponibil în acest mediu); Faza 8 (UI diagnostics dedicat); Faza 9 (export schemaVersion 3); validarea inbound peer-association (punctul 12); răspunsul automat la o ofertă `m=message` inițiată de peer.

**26. Probleme client identificate**: niciuna nouă — comentariul arhitectural din W100 (`MsrpSipIntegration.h`, `docs/msrp-foundation.md`) care afirma greșit că `onCallSdpCreated` e read-only a fost corectat.

**27. Probleme server identificate**: niciuna — nu a existat acces la server pentru a re-produce/verifica raportul anterior de "peer connection not found".

**28. Limitări rămase**:
- Răspunsul (answer) la o ofertă `m=message` inițiată de peer nu injectează automat un accept/reject la indexul corect — necesită un audit separat al modului în care PJSUA gestionează tipuri de media pe care nu le recunoaște, pentru a nu risca coruperea SDP-ului sau încălcarea RFC 3264.
- Validarea explicită a asocierii conexiunilor inbound (To-Path/session-id la primul SEND) nu e implementată (punctul 12).
- Corelarea `messageDeliveryStatusChanged` (MSRP) în `deliveryState`-ul din `MessageHistoryStore` e doar logată, nu scrisă — necesită un câmp de corelare dedicat, diferit de spațiul Message-ID SIP MESSAGE.
- UI dedicat de diagnostics (Faza 8: selected/negotiated/actual transport, fallback reason, session/transaction/REPORT/IMDN status vizibile în interfață) nu a fost construit.
- Export `schemaVersion = 3` (Faza 9) nu a fost implementat.
- Nicio validare live (SIP-Server-RTT/Blink/AG Projects/Linphone) nu a fost executată — mediul de lucru nu are acces la niciunul dintre acestea.

**29. Ce rămâne pentru W102**: implementarea răspunsului automat la oferte `m=message` primite de la peer (Faza 2 completă bidirecțional); validarea/hardening-ul asocierii conexiunilor inbound; corelarea completă a statusului de livrare MSRP în Message History; UI de diagnostics (Faza 8); export `schemaVersion = 3` cu `transportDecision`/`msrpSession`/`peerAssociation`/`fallbackReason`/`negotiationState`; execuția testelor manuale live cu SIP-Server-RTT/Blink/AG Projects/Linphone de îndată ce un mediu cu acces la acestea devine disponibil.

---

## Commiturile create

```
9531e8b feat(msrp): inject negotiated msrp media into live sip sdp
dbec4ba feat(msrp): correlate sip dialogs and msrp sessions
540cc74 feat(messaging): wire msrp transport policy to composer
a775f06 feat(msrp): close the send-response-report-history loop
75c6b31 docs(interop): document lmpe and msrp validation
```

(plus acest fișier de raport, într-un commit final)

## Git status

Toate modificările din surse sunt commise pe `feature/w101-live-msrp-sip-integration`. Fișierele `.bat` locale de build/test (`build_*.bat`, `ctest_*.bat`, `rebuild_pjsua2.bat`) rămân netrackate — sunt helper-e locale de dezvoltare, nu livrabile ale task-ului, conform practicii deja existente pe această ramură dinainte de W101.

## Push status

`git push -u origin feature/w101-live-msrp-sip-integration` — vezi confirmarea din mesajul următor al agentului (rulat imediat după acest commit).

## Confirmare

**Nu s-a făcut merge în `main` sau `release`.** Toate modificările există exclusiv pe `feature/w101-live-msrp-sip-integration`.
