# Task W100 — MSRP Foundation — Result

1. **Branch**: `feature/w100-msrp-foundation`

2. **Branch de pornire**: `feature/w099-xcap-foundation`

3. **Versiune veche/nouă**: `CMakeLists.txt` (`project(SIPClient VERSION
   0.1.0 ...)`) rămâne `0.1.0`, neschimbată — conform convenției stabilite
   în W090–W099, se actualizează doar `docs/project-status.md` (counter
   53→54).

4. **Schema JSON veche/nouă**: `schemaVersion` rămâne **2**.
   `InteropTraceExporter` adaugă două array-uri noi la nivel de rădăcină
   (`msrpSessions`, `msrpEvents`) și patru câmpuri aditive pe fiecare
   eveniment existent din `events` (`selectedTransport`,
   `actualTransport`, `fallbackUsed`, `fallbackReason`) — nimic existent nu
   a fost redenumit/eliminat, deci modificarea e complet aditivă.

5. **Fișiere modificate/adăugate**: 26 fișiere noi în `src/msrp/`
   (`MsrpTypes.h`, `MsrpSessionInfo.h`, `MsrpPath.h/.cpp`,
   `MsrpSdpNegotiator.h/.cpp`, `MsrpFrame.h`, `MsrpFrameParser.h/.cpp`,
   `MsrpFrameSerializer.h/.cpp`, `MsrpChunkAssembler.h/.cpp`,
   `MsrpMessageChunker.h/.cpp`, `MsrpTransaction.h`,
   `MsrpTransactionStore.h/.cpp`, `MsrpSessionStore.h/.cpp`,
   `MsrpDiagnosticsEvent.h`, `MsrpDiagnosticsStore.h/.cpp`,
   `MsrpTransport.h`, `MsrpTcpTransport.h/.cpp`, `MsrpTlsTransport.h/.cpp`,
   `MsrpSession.h/.cpp`, `MsrpPayloadDispatcher.h/.cpp`,
   `MessagingTransportPolicy.h/.cpp`, `MsrpSipIntegration.h/.cpp`);
   `src/gui/panels/MsrpPage.h/.cpp`; 5 documente noi
   (`docs/msrp-foundation.md`, `-transport.md`, `-protocol.md`,
   `-security.md`, `-testing.md`) + agent-prompt/result; 11 fișiere de
   test noi (`tests/test_msrp_*.cpp` ×10,
   `test_messaging_transport_policy.cpp`). Modificate: `AppSettings.h`
   (`msrp/*` settings), `InteropTraceExporter.h/.cpp`, `NavRail.cpp`,
   `MainWindow.h/.cpp` (nou "MSRP" nav page, index 6, `kPageCount` 10→11),
   `SipCall.cpp` (un singur apel read-only către
   `MsrpSipIntegration::detectFromSdp`), `CMakeLists.txt`,
   `tests/CMakeLists.txt`, `docs/msrp-diagnostics.md`,
   `docs/sip-message.md`, `docs/imdn.md`, `docs/is-composing.md`,
   `docs/windows-trace-json-export.md`, `docs/project-status.md`.

6. **Decizia arhitecturală MSRP**: fundație completă construită pe Qt
   (`QTcpSocket`/`QSslSocket`), complet independentă de pjsua2 — vezi
   punctul 7. Straturi separate (nicio clasă monolitică): negociere SDP
   (`MsrpSdpNegotiator`), path (`MsrpPath`), model sesiune
   (`MsrpSessionInfo`), state machine (`MsrpSession`), transport
   (`MsrpTransport`/`MsrpTcpTransport`/`MsrpTlsTransport`), frame codec
   (`MsrpFrame`/`MsrpFrameParser`/`MsrpFrameSerializer`), chunking
   (`MsrpMessageChunker`/`MsrpChunkAssembler`), tranzacții
   (`MsrpTransactionStore`), payload dispatch (`MsrpPayloadDispatcher`),
   politică transport (`MessagingTransportPolicy`), diagnostics
   (`MsrpDiagnosticsStore`), integrare SIP read-only
   (`MsrpSipIntegration`).

7. **Suportul PJSIP reutilizat / ce a fost implementat separat**: audit
   confirmat — acest build vendorizat de pjproject **nu are niciun
   suport MSRP** (căutare case-insensitive "msrp" în toate header-ele din
   `.deps/pjsip-msvc-install/include` → zero rezultate). SDP-ul de
   offer/answer e generat de pjsua2's high-level `Call::makeCall()`, fără
   punct de injecție manual. `Call::onCallSdpCreated` este **read-only**
   (`SdpSession::wholeSdp` nu are `toPj()` invers) — editarea lui nu ar
   ajunge în SDP-ul real trimis, iar manipularea directă a pointerului
   `pjSdpSession` ar fi exact genul de tranzacție SIP manuală fragilă
   interzisă de convențiile proiectului (aceeași decizie ca la Publish în
   W098 și Digest în W099). **Decizie**: 100% din stack-ul MSRP
   (negociere SDP ca text pur, transport, frame codec, chunking,
   tranzacții) e implementat independent în Qt; singura integrare cu
   apelurile SIP live este **detecție read-only** (o singură linie
   adăugată în `onCallSdpCreated`, care nu atinge niciodată `prm.sdp`).

8. **Modelul sesiunii**: `MsrpSessionInfo` — identitate (sessionKey,
   sipCallId, localSessionId/remoteSessionId, local/remotePath),
   negociere (transport, setup, connection, direction, acceptTypes/
   WrappedTypes, fileSelector/Disposition), lifecycle (11 stări distincte,
   niciodată "established" doar pentru că SDP conține m=message), rol
   (active-connector/passive-listener/holdconn/unknown), diagnostic
   (timestamps, parseStatus, warnings, bytes/frames/messages counters,
   lastError).

9. **Negocierea SDP implementată**: `MsrpSdpNegotiator::parseMessageBlocks()`
   — parsează toate secțiunile `m=message` (ordine-independentă pentru
   atribute), suportă `a=path`, `a=accept-types`, `a=accept-wrapped-types`,
   `a=setup`, `a=connection`, `a=sendrecv/sendonly/recvonly/inactive`,
   `a=file-selector/disposition/transfer-id`, multiple secțiuni
   `m=message`, `port=0` (marcat `rejected`, nu fatal). `buildOfferBlock()`
   generează o secțiune `m=message` din componente furnizate de
   apelant (host/port/session-id niciodată hardcodate).

10. **Setup active/passive/actpass/holdconn**:
    `negotiateRole(localSetup, remoteSetup)` — `active+passive`/
    `passive+active` → rol valid; `active+active`/`passive+passive` →
    eroare explicită (niciodată o alegere silențioasă); `holdconn` pe
    oricare parte → rol `HoldConn`, fără conectare; `actpass` nerezolvat →
    eroare explicită ("negociere incompletă").

11. **TCP transport**: `MsrpTcpTransport` — mod activ
    (`QTcpSocket::connectToHost` + timeout configurabil), mod pasiv
    (`QTcpServer::listen` cu port automat/fix, accept timeout, un singur
    peer acceptat apoi listener închis), fără `waitForConnected`/
    `waitForReadyRead` (asincron complet, rulează pe Qt event loop), buffer
    limitat la 8 MiB (backpressure).

12. **TLS transport și validarea certificatelor**: `MsrpTlsTransport` —
    `QSslSocket::connectToHostEncrypted(host,port,host)` (mod activ, SNI +
    verificare hostname), server-side prin `setSocketDescriptor` +
    `startServerEncryption()` (mod pasiv). `msrpTlsVerifyPeer` implicit
    `true`; `ignoreSslErrors()` apelat **doar** când verificarea e
    explicit dezactivată — niciodată implicit. `msrpTlsCaPath` opțional
    pentru CA-uri suplimentare. **Limitare documentată**: provizionarea
    certificat/cheie server-side pentru `listenAsPassive()` nu are UI de
    producție (utilizabilă doar pentru harness local).

13. **Parserul MSRP**: `MsrpFrameParser` — incremental, binary-safe
    (scanare `QByteArray::indexOf`, niciodată conversie text a bufferului
    întreg), gestionează start-line/header/body/delimiter fragmentate pe
    mai multe citiri, mai multe frame-uri per citire, body cu NUL/CRLF/
    bytes ≥0x80, transaction-id invalid, headere necunoscute/duplicate
    (tolerate, ultima valoare câștigă), frame supradimensionat
    (`LimitExceeded` verificat înainte de acumulare nelimitată), recovery
    după frame invalid (caută următorul `"MSRP "` plauzibil).

14. **Serializerul MSRP**: `MsrpFrameSerializer` — CRLF/delimiter exacte,
    validează fiecare valoare de header (inclusiv headere necunoscute)
    pentru `\r`/`\n`/NUL înainte de scriere — respinge (ieșire goală,
    `ok=false`) orice ar permite header injection; body binary-safe
    (niciodată prin `QString`), testat round-trip pentru conținut binar
    arbitrar.

15. **SEND și răspunsuri**: `MsrpSession::handleFrame()` răspunde cu
    200/400 la fiecare chunk SEND bine format; `MsrpTransactionStatus`
    separă explicit `Accepted` (răspunsul 200 la SEND) de
    `ReportedSuccess`/`ReportedFailure` (REPORT) — niciodată confundate
    între ele sau cu un IMDN delivered/displayed (nivel de payload,
    tratat separat de `MsrpPayloadDispatcher`).

16. **REPORT și statusuri**: trimis doar după reasamblarea completă a
    mesajului, doar dacă ultimul chunk a cerut `Success-Report: yes`;
    corelat cu tranzacția originală prin Message-ID via
    `MsrpTransactionStore`.

17. **Transaction-id și Message-ID**: transaction-id generat aleator per
    chunk (niciodată reutilizat); Message-ID generat aleator per mesaj
    trimis, folosit pentru corelarea REPORT-urilor și pentru reasamblarea
    chunk-urilor.

18. **Byte-Range și chunking**: `MsrpMessageChunker::buildSendFrames()`
    (pur, outbound) — Byte-Range corect (`start-end/total`), chunk final
    marcat `$`, restul `+`. `MsrpChunkAssembler` (inbound) — reasamblare
    prin Message-ID, detectare duplicat (idempotent), gap/overlap
    (buferat cu avertisment — reasamblare RFC-perfectă pentru reordonare
    adversarială e o limitare documentată), plafon `maxMessageBytes`
    aplicat **înainte** de orice alocare mare.

19. **Continuation +/$/#**: implementat complet în model, parser,
    serializer, chunker și assembler; `#` (abort) elimină imediat
    asamblarea în curs pentru acel Message-ID.

20. **Payload-uri suportate**: text/plain, text/html, message/cpim
    (unwrapped), message/imdn+xml, application/im-iscomposing+xml,
    conținut necunoscut (diagnostics-only dacă pare binar, altfel
    afișat ca text).

21. **Integrarea CPIM**: `MsrpPayloadDispatcher` apelează `CpimParser::parse`
    nemodificat, reclasifică Content-Type-ul intern, păstrează metadata
    CPIM doar pentru preview.

22. **Integrarea IMDN**: `ImdnParser::parse` nemodificat +
    `MessageHistoryStore::appendInboundImdn`/`correlateDelivery` —
    exact aceleași apeluri ca pentru SIP MESSAGE.

23. **Integrarea is-composing**: `IsComposingParser::parse` nemodificat +
    `MessageHistoryStore::appendInboundTyping` — actualizează aceleași
    rânduri de istoric ca SIP MESSAGE.

24. **Politica MSRP/SIP MESSAGE fallback**: `MessagingTransportPolicy` —
    patru moduri (SIP MESSAGE only / MSRP preferred / MSRP required /
    automatic), decizie pură testată complet (10 teste). **Nu este încă
    conectată în calea reală de compunere-și-trimitere** — `SipManager`
    trimite în continuare mereu prin SIP MESSAGE; politica există și
    funcționează izolat, gata pentru integrare viitoare.

25. **Integrarea cu apelurile audio/video/RTT**: niciun cod nou nu
    modifică generarea SDP pentru audio/video/RTT. Singura integrare este
    `MsrpSipIntegration::detectFromSdp()`, apelată dintr-un hook existent,
    read-only, care nu atinge niciodată `prm.sdp` — imposibil să afecteze
    media negociată.

26. **UI adăugat**: pagină nouă "MSRP" (`src/gui/panels/MsrpPage.h/.cpp`)
    — configurare completă, sesiune de test manuală (experimentală,
    independentă de apelurile live), tabel sesiuni active (populat atât
    de sesiunea de test cât și de detecțiile din apeluri SIP reale),
    jurnal diagnostics, export JSON/TXT.

27. **Integrarea în ladder**: MSRP este trafic TCP simplu, nu SIP — conform
    cerinței explicite, **nu** este desenat în SIP Ladder; jurnalul de
    diagnostics din pagina MSRP este echivalentul dedicat.

28. **Diagnostics/stores**: `MsrpSessionStore` (singleton global,
    upsert/snapshot/clear/signals), `MsrpDiagnosticsStore` (singleton
    global, mărginit la 1000 intrări), `MsrpTransactionStore`
    (instanțiabil, unul per sesiune, mărginit la 1000 tranzacții
    finalizate reținute).

29. **Export JSON/TXT**: `msrpSessions` + `msrpEvents` noi, plus 4 câmpuri
    aditive pe `events` — vezi punctul 4. Export TXT cu sumar sesiuni +
    timeline tranzacții, path-uri redactate, fără body binar brut.

30. **Configurarea introdusă**: toate cele ~19 setări din secțiunea B a
    task-ului (`enableMsrp`, `enableMsrpTcp/Tls`, `preferMsrp`,
    `allowSipMessageFallback`, `msrpLocalBindAddress`/`AdvertisedHost`,
    `msrpPortMode`/`FixedPort`, `msrpConnectionTimeoutMs`/
    `TransactionTimeoutMs`/`IdleTimeoutSeconds`, `msrpMaxFrameBytes`/
    `MaxMessageBytes`/`ChunkSizeBytes`/`MaxConcurrentSessions`,
    `msrpRequestReports`, `msrpAcceptTypes`/`WrappedTypes`,
    `msrpTlsVerifyPeer`/`CaPath`, `msrpExperimental`), plus
    `messagingTransportMode` — toate cu default conservator (MSRP
    dezactivat, fallback SIP MESSAGE activ, fără host implicit, verify
    TLS activ).

31. **Limitele de securitate**: dimensiune frame/mesaj plafonată înainte de
    alocare, header injection respins la serializare, path-uri redactate
    peste tot (diagnostics + export), TLS verify implicit activ, nicio
    parolă/cheie/certificat privat logat sau exportat.

32. **Teste unitare rulate**: 11 ținte noi de test (SDP, path, frame
    parser, frame serializer, chunker, chunk assembler, transaction
    store, transport policy, payload dispatcher) + harness — toate
    incluse în `ctest --output-on-failure` cu `ENABLE_PJSIP=ON`:
    **100% tests passed, 0 failed, 66 total** (zero regresii W090–W099).

33. **Teste de integrare/harness**: `test_msrp_session_harness` — două
    `MsrpSession` (activ + pasiv) comunicând peste `127.0.0.1` cu port
    OS-assigned, testat: conectare reciprocă la `Established`, SEND
    text/plain primit byte-cu-byte, mesaj chunked (3000 bytes, chunk 256)
    reasamblat corect, închidere conexiune observată. Nu acoperă (gap
    documentat): REPORT explicit assertion, abort mid-transfer, TLS peste
    loopback, injectare de frame invalid printr-un socket real (acoperit
    separat, fără socket, în `test_msrp_frame_parser`).

34. **Test manual cu SIP-Server-RTT**: documentat în
    `docs/msrp-testing.md` cu placeholdere; **neexecutat** — niciun server
    disponibil în această sesiune.

35. **Ce a fost validat real**: întregul protocol (parser/serializer/
    chunking/tranzacții) prin teste unitare exhaustive; un schimb real
    client-server MSRP peste TCP loopback (harness); detecția SDP
    read-only prin teste pe text SDP sintetic.

36. **Ce NU a putut fi validat**: interoperabilitate reală cu
    SIP-Server-RTT/Blink/AG Projects (niciun server disponibil);
    injectarea live a SDP MSRP într-un apel real (arhitectural
    imposibil/nesigur cu acest build pjproject — vezi punctul 7); TLS
    peste o conexiune reală (doar mecanismul e implementat, netestat
    end-to-end cu certificate reale).

37. **Ce rămâne pentru W101 LMPE Interoperability**: encodare/decodare
    LMPE peste MSRP (explicit în afara scopului acestui task); conectarea
    `MessagingTransportPolicy` în calea reală de trimitere a mesajelor;
    eventual, dacă devine necesar, un pjproject rebuild cu suport MSRP
    nativ pentru a permite injectare SDP live.

38. **Limitări cunoscute**: vezi `docs/msrp-foundation.md` §12 — fără
    injectare SDP live; politica de transport neconectată la trimiterea
    reală; reasamblare chunk-uri best-effort pentru reordonare
    adversarială; fără UI de provizionare certificat server-side TLS.

39. **Commituri**: `feat(msrp): add session and negotiation models`,
    `feat(msrp): implement msrp sdp offer answer`,
    `feat(msrp): add path parser and session lifecycle`,
    `feat(msrp): implement frame parser and serializer`,
    `feat(msrp): implement tcp and tls transports`,
    `feat(msrp): add send report and transaction handling`,
    `feat(msrp): implement chunking and byte range assembly`,
    `feat(msrp): dispatch cpim imdn and is-composing payloads`,
    `feat(messaging): add msrp transport policy and sip fallback`,
    `feat(ui): add msrp session and diagnostics pages`,
    `feat(export): include msrp sessions and protocol events`,
    `test(msrp): add protocol and transport validation`,
    `docs(msrp): document msrp foundation workflow`.

40. **git status**: curat după commit, cu excepția scripturilor locale
    auxiliare `build_w098.bat`/`build_w099.bat`/`ctest_w098.bat`/
    `ctest_w099.bat`/`ctest_msrp.bat` (intenționat necomise).

41. **Push status**: `git push -u origin feature/w100-msrp-foundation` —
    branch nou pe remote, urmărire configurată.

42. **Confirmare**: **fără merge în main/release.**
