# Task-W102 — MSRP Live Interoperability — Final Report

## 1. Branch nou

`feature/w102-msrp-live-interoperability`

## 2. Branch de pornire

`feature/w101-live-msrp-sip-integration`

## 3. Versiune veche/nouă

Nu există un fișier de versiune semantică dedicat în acest repository
(vezi task-urile anterioare); "versiunea" funcțională relevantă este
capacitatea MSRP: veche = "doar offerer path, fără answer la oferte
peer-initiated" (W101); nouă = "offer/answer bidirecțional cu roluri
active/passive negociate real, export v3, UI diagnostics extinsă,
comparator client/server" (W102).

## 4. Schema JSON veche/nouă

`schemaVersion` 2 → **3**. Câmpuri noi (aditive, compatibile) pe fiecare
intrare `msrpSessions`: `role`, `remoteSetup`, `negotiationState`,
`peerAssociation` (`{method, mediaIndex, sipHeaderCallId, confidence}`).
Toate câmpurile v2 rămân neschimbate. Vezi
[windows-trace-json-export.md](../windows-trace-json-export.md).

## 5. Fișiere modificate

- `src/msrp/MsrpSipMediaInjector.h`, `.cpp` — funcție nouă
  `answerMessageMediaAtIndex`.
- `src/sip/SipCall.cpp` — ramura de answer în `onCallSdpCreated`,
  `extractRemoteMessageBlocks`, `createMsrpSessionObject`/
  `startMsrpPassiveListener`/`startMsrpActiveConnect` (refactorizare din
  `ensureMsrpSessionForOutgoingSdp`).
- `src/sip/InteropTraceExporter.h`, `.cpp` — schemaVersion 3.
- `src/sip/TraceComparator.h`, `.cpp` (nou) — comparator client/server.
- `src/gui/panels/MsrpPage.cpp` — coloane UI noi.
- `CMakeLists.txt`, `tests/CMakeLists.txt` — `TraceComparator.cpp`, target
  nou `test_trace_comparator`.
- `tests/test_msrp_sip_media_injector.cpp`, `tests/test_windows_trace_json_export.cpp`,
  `tests/test_trace_comparator.cpp` (nou).
- Documentație: vezi secțiunea de mai jos.

## 6. Offer local

Neschimbat față de W101 — vezi
[msrp-live-sdp-integration.md](../msrp-live-sdp-integration.md). Neretestat
suplimentar în W102 dincolo de suita de regresie (67→68 teste, toate PASS).

## 7. Answer local

**Implementat nou în W102.** `onCallSdpCreated`'s answer branch (`prm.remSdp`
populat) extrage secțiunile `m=message` din oferta peer-ului
(`extractRemoteMessageBlocks`, reutilizând parserul deja testat
`MsrpSdpNegotiator::parseMessageBlocks`), și înlocuiește placeholder-ul
rejectat (port 0) pe care PJSIP îl generează deja automat la indexul corect
(`pjmedia_sdp_neg.c`'s `create_answer`, verificat prin citirea sursei) cu o
secțiune reală acceptată, via `MsrpSipMediaInjector::answerMessageMediaAtIndex`.
Dovedit la nivel de bytes-pe-wire în `test_msrp_sip_media_injector.cpp`
(3 teste noi).

## 8. Peer-initiated offer

Vezi punctul 7 — este exact acest scenariu. O a doua secțiune `m=message`
simultană în aceeași ofertă este respinsă explicit (documentat, nu
silențios) — modelul rămâne "o singură sesiune MSRP live per apel"
(decizie arhitecturală din W101, nemodificată).

## 9. re-INVITE/UPDATE

Dacă `msrpSession` există deja (dintr-o negociere anterioară), o
re-ofertă peer-initiată ulterioară reutilizează aceeași sesiune/rol, doar
re-răspunde la orice index plasează noua ofertă `m=message`. Nu a fost
testat live printr-o secvență INVITE→re-INVITE reală (nu există harness
PJSIP local pentru asta) — verificat doar prin inspecția codului și fixture
de unitate ale formelor SDP implicate.

## 10. Hold/resume

Nemodificat față de fluxul existent de hold/resume (audio/video/RTT,
W095) — MSRP nu este afectat de hold-ul audio/video întrucât sunt secțiuni
`m=` independente. Netestat live specific pentru interacțiunea
hold-audio + MSRP-activ.

## 11. Roluri active/passive/actpass

**Implementat.** Rolul răspunsului este întotdeauna complementul structural
al propunerii remote-ului (`active`↔`passive`; `actpass`/necunoscut →
`passive` implicit). Aceasta exclude prin construcție coliziunile
active/active și passive/passive — nu există cod care să aleagă independent
un rol fix pe partea de answer. Când rolul negociat cere ca acest client să
fie partea activă (conectoare), `SipCall::Impl::startMsrpActiveConnect`
folosește `MsrpSession::connectAsActive` cu `a=path`-ul remote-ului.
**Limitare cunoscută, documentată**: pe partea de ofertă (acest client este
offerer), oferta rămâne întotdeauna `a=setup:actpass` și ascultă pasiv
necondiționat; dacă remote-ul răspunde cu `a=setup:passive` (ambele părți
pasive), acest client nu inspectează încă răspunsul remote-ului pentru a
reacționa activ-conectând — vezi [msrp-offer-answer.md](../msrp-offer-answer.md).

## 12. Incoming peer association

**Nemodificat structural față de W101, extins cu `role` real (exportat).**
Fiecare `SipCall` deține exact o sesiune/listener dedicat(ă) — coliziunile
la nivel de port sunt structural imposibile. Validarea explicită
To-Path/session-id a conexiunii primite (no-match/ambiguous/stale/
role-mismatch etc., cerută de Faza 4) **nu este implementată** — documentat
ca gap în [msrp-peer-association.md](../msrp-peer-association.md), moștenit
din W101 și încă nerezolvat.

## 13. Multiple calls

Structural susținut (fiecare `SipCall`/`MsrpSession` e independent) —
confirmat prin audit de cod, **netestat live** (fără mediu multi-apel real
disponibil).

## 14. Early dialogs

Auditat — vezi [msrp-early-dialogs-and-forking.md](../msrp-early-dialogs-and-forking.md).
Niciun cod nou; concluzia este că modelul `pjsua2::Call` unic previne
structural problema, dar nu a fost testat live.

## 15. Forking

**NOT RUN** — niciun proxy/fixture capabil de forking real nu a fost
disponibil în acest mediu. Vezi documentul de mai sus.

## 16. TCP

Neschimbat (W100). Regresie confirmată prin suita de teste.

## 17. TLS

Auditat — vezi [msrp-tls.md](../msrp-tls.md). Nicio schimbare de cod
necesară; verificat că implementarea W100 satisface deja cerințele W102
(certificate configurabile, verificare hostname, CA trust, fără dezactivare
implicită, timeout-uri, erori redactate). Gap-ul de provisioning
certificat server-side (pasiv) rămâne documentat, nerezolvat.
**Niciun handshake TLS live nu a fost efectuat** (fără peer TLS-capabil
disponibil).

## 18. Transport policy

Neschimbat față de W101 — reauditat, confirmat corect.

## 19. Fallback

Neschimbat față de W101 — reauditat, confirmat corect (fără stare
persistentă, deci fără fallback permanent, fără retransmisie dublă).

## 20. Recovery

Consecință structurală a deciziei recalculate la fiecare trimitere — vezi
punctul 19. Netestat live (fără eșec real de socket împotriva unui peer
real).

## 21. UI diagnostics

**Extins parțial.** Pagina MSRP arată acum coloane noi "Negotiation" și
"Peer Association" pe tabelul de sesiuni, populate din câmpurile reale
(`offerAnswerState`, `mediaIndex`+`sipHeaderCallId`). Lista completă cerută
de Faza 9 (listener state, socket state, TLS state, connection ID,
reconnect count, session expiry etc.) **nu a fost implementată integral** —
scop redus deliberat pentru a prioritiza corectitudinea protocolului
(Fazele 2–4) în timpul disponibil; rămâne pentru W103.

## 22. Export JSON v3

**Implementat.** Vezi punctul 4. `msrpTransactions`, `fallbackEvents`,
`interopValidation`, `tlsDiagnostics` (cerute de spec) **nu au fost
adăugate** — documentat explicit în `InteropTraceExporter.h` de ce (fără
store global de tranzacții agregat, fără istoric persistent de decizii de
fallback, fără al doilea export real de comparat pentru validare).

## 23. Comparator

**Implementat nou.** `TraceComparator::compareTraces` — funcție pură,
deterministă, corelează pe `(callId, cseq)`, detectează
missingOnLeft/Right, duplicate, directionMismatch, payloadTypeMismatch,
timestampOutOfTolerance. 8 teste unitare, toate PASS. Nu a fost rulat
împotriva unui export real server — repository-ul nu conține formatul de
export SIP-Server-RTT (aceeași situație documentată de W094).

## 24. SIP-Server-RTT

**NOT RUN** — niciun server accesibil din acest mediu de dezvoltare.

## 25. Blink

**NOT RUN** — niciun client/mediu disponibil.

## 26. AG Projects

**NOT RUN** — niciun client/mediu disponibil.

## 27. Linphone

**NOT RUN** — niciun client/mediu disponibil; nu se pretinde interoperabilitate.

## 28. PASS

Toate cele 68 ținte CTest (67 moștenite din W101 + `test_trace_comparator`
nou), inclusiv cele 3 teste noi de answer-at-index în
`test_msrp_sip_media_injector`, testul nou de câmpuri v3 în
`test_windows_trace_json_export`, și cele 8 teste noi ale comparatorului.
Build complet (Debug, `ENABLE_PJSIP=ON`) fără erori/avertismente noi.

## 29. FAIL

Niciunul.

## 30. BLOCKED

Niciun element marcat explicit `BLOCKED` — elementele care necesitau un
mediu extern indisponibil sunt raportate `NOT RUN` (nu au fost încercate și
eșuate; pur și simplu nu exista un peer/server de testat).

## 31. NOT RUN

SIP-Server-RTT (punctele 24, live end-to-end MSRP), Blink (25), AG Projects
(26), Linphone MSRP (27), forking live (15), handshake TLS live (17),
multi-call live (13), re-INVITE/hold live cu MSRP activ (9, 10).

## 32. Probleme client

Niciuna nouă descoperită. Limitările cunoscute (rol activ nedetectat pe
partea de ofertă la răspuns "passive", incoming-connection validation
lipsă, o singură sesiune MSRP per apel) sunt documentate mai sus și în
fișierele linkate, nu ascunse.

## 33. Probleme server

Nu s-a putut testa contra niciunui server — nicio problemă de server
identificată sau exclusă.

## 34. Teste automate

68/68 CTest PASS. Detaliu în punctul 28.

## 35. Teste manuale

Toate `NOT RUN` — vezi punctele 24–27.

## 36. Regresii

Zero — suita completă W090–W101 (67 ținte) rulează neschimbată, plus noua
țintă. Niciun test existent modificat în comportament, cu excepția unei
singure asertări hardcodate (`schemaVersion == 2` → acum
`InteropTraceExporter::kSchemaVersion`) care testa o valoare stale, nu
comportament.

## 37. Limitări rămase

- Fără validare a conexiunii MSRP primite dincolo de asocierea structurală
  prin listener dedicat (To-Path/session-id matching neimplementat).
- O singură secțiune `m=message` acceptată per apel (a doua e respinsă
  explicit).
- Partea de ofertă (acest client offerer) nu reacționează la un răspuns
  remote "passive-only" (rămâne pasiv, sesiune nefuncțională în acel caz
  rar).
- UI diagnostics parțială (2 coloane noi, nu lista completă din Faza 9).
- Export v3 nu include `msrpTransactions`/`fallbackEvents`/
  `interopValidation`/`tlsDiagnostics`.
- Comparator nu a fost validat contra unui export server real.
- TLS server-side (pasiv) provisioning certificat rămâne neimplementat.
- Nicio testare live (server/clienți reali) nu a fost posibilă în acest
  mediu.

## 38. Ce rămâne pentru W103

Aceste elemente rămân valabile ca bază pentru W103 (LMPE Foundation) —
LMPE însuși nu depinde de ele, dar hardening-ul MSRP rămas ar trebui
adresat fie înainte, fie în paralel, conform judecății echipei:
validare conexiune primită (To-Path/session-id), suport multi-sesiune per
apel, reacție la rol "passive-only" pe partea de ofertă, UI diagnostics
completă, `msrpTransactions`/`fallbackEvents`/`interopValidation`/
`tlsDiagnostics` în export, testare live cu SIP-Server-RTT/Blink/AG
Projects/Linphone de îndată ce un mediu devine disponibil.

## 39. Commiturile create

```
e6ef45c feat(msrp): answer peer-initiated m=message offers with real role negotiation
c51fbe2 feat(export): bump interop trace schema to v3 with real MSRP role/peer-association fields
594f7c4 feat(ui): show MSRP negotiation state and peer-association result on the sessions table
55a60af feat(interop): add deterministic client/server trace comparator
```

(plus commit-ul de documentație/raport final care urmează acestui fișier)

## 40. Git status

Curat după commit-ul de documentație (verificat înainte de push).

## 41. Push status

`git push -u origin feature/w102-msrp-live-interoperability` executat cu
succes (vezi confirmarea din conversație).

## 42. Confirmare fără merge

```
Branch-ul a fost push-uit.
Nu s-a făcut merge în main.
Nu s-a făcut merge în release.
Nu au fost modificate sursele pjproject.
Toate rezultatele de interoperabilitate sunt raportate numai pe baza testelor executate — orice scenariu live neexecutat este marcat explicit NOT RUN, niciodată presupus PASS.
```
