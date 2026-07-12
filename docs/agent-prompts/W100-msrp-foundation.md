# Task-W100 — MSRP Foundation (agent prompt, verbatim)

Repo: https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Branch: `feature/w100-msrp-foundation`
Pornește din: `feature/w099-xcap-foundation`

## Context

Clientul Windows suportă deja: audio, video, RTT, SIP MESSAGE, text/plain
și text/html, CPIM, IMDN, is-composing, Presence, XCAP Foundation,
Messaging/MSRP Diagnostics, SIP/SDP ladder, export JSON interop
schemaVersion 2.

Serverul SIP-Server-RTT are fundație MSRP și relay MSRP, însă
interoperabilitatea completă cu Blink/AG Projects nu este încă validată.

## Obiectiv

Implementează fundația completă MSRP în clientul Windows: negociere
SIP/SDP; sesiune MSRP; transport TCP și TLS; parser și serializer MSRP;
SEND și REPORT; chunking și Byte-Range; text/plain, CPIM, IMDN și
is-composing peste MSRP; integrare în UI, diagnostics, export și ladder;
fallback configurabil către SIP MESSAGE.

Nu implementa LMPE în acest task. LMPE rămâne pentru W101.

## Reguli permanente

1. Nu hardcoda IP-uri, domenii, porturi, useri, parole, URI-uri, path-uri
   MSRP sau certificate.
2. Toate setările trebuie să provină din profil/config/UI.
3. Nu rupe audio/video/RTT, SIP MESSAGE, IMDN, is-composing, Presence sau
   XCAP.
4. MSRP trebuie să fie dezactivat implicit.
5. Orice componentă experimentală trebuie să aibă flag UI/config.
6. Nu bloca UI thread.
7. Nu introduce acces la rețea în parsere sau modele.
8. Nu duplica parserele CPIM/IMDN/is-composing existente.
9. Nu face merge în main/release.
10. Implementează incremental și păstrează aplicația buildable după
    fiecare etapă majoră.

## A. Audit și decizie arhitecturală

Înainte de implementare: inspectează versiunea PJSIP/pjsua2, suportul real
pentru MSRP disponibil în pjproject, callback-urile SIP/SDP existente,
modelul de call și media, threading-ul Qt/PJSIP, infrastructura TLS și
certificate, parserele și modelele W090–W099. Decide și documentează dacă
transportul MSRP folosește suport pjproject existent sau un transport
propriu peste QTcpSocket/QSslSocket, cum este asociată o sesiune MSRP cu un
dialog SIP, cum sunt corelate offer/answer, socket-ul și mesajele MSRP,
cine deține lifecycle-ul obiectelor. Separă clar SDP negotiation, session
state, TCP/TLS transport, MSRP frame codec, message/chunk assembly,
payload dispatch, diagnostics/UI — nu crea o clasă monolitică.

Arhitectură recomandată, adaptabilă după audit: MsrpSessionInfo,
MsrpSdpNegotiator, MsrpSession, MsrpTransport, MsrpTcpTransport,
MsrpTlsTransport, MsrpFrameParser, MsrpFrameSerializer,
MsrpChunkAssembler, MsrpTransactionStore, MsrpSessionStore,
MsrpDiagnosticsStore, MsrpPayloadDispatcher.

## B–Z (rezumat)

Configurație extinsă (enableMsrp, enableMsrpTcp/Tls, preferMsrp,
allowSipMessageFallback, msrpLocalBindAddress/AdvertisedHost, portMode,
timeouts, limite de dimensiune, acceptTypes/WrappedTypes, TLS
verify/CA, msrpExperimental) — implicit dezactivat, fallback SIP MESSAGE
implicit activ. Model complet de sesiune MSRP cu identitate, negociere,
lifecycle (disabled→detected→offered→answered→negotiated→connecting→
connected→established→disconnecting→closed/failed), rol, diagnostic.
Negociere SDP extinsă (m=message, path, accept-types/wrapped-types,
setup, connection, direction, file-selector/disposition, multiple
m=message, re-INVITE/UPDATE, port 0, actpass/active/passive/holdconn cu
validare). Parser/builder MSRP path + session-id criptografic. Transport
TCP (client/server mode, timeout, fără blocare) și TLS (verificare
certificat, CA, SNI). Model, parser (incremental, binary-safe) și
serializer (CRLF corect, fără header injection) de frame MSRP. SEND,
răspunsuri, REPORT cu statusuri de tranzacție distincte. Chunking +
Byte-Range cu MsrpChunkAssembler (limite de memorie). Payload dispatch
reutilizând CpimParser/ImdnParser/IsComposingParser/MessageHistoryStore/
MessagingEventStore — fără duplicare. Politică explicită
MessagingTransportPolicy (SIP MESSAGE only / MSRP preferred / MSRP
required / automatic) cu fallback controlat. Integrare cu apelurile
audio/video/RTT fără a le rupe. UI "MSRP" (configurare, sesiuni active,
integrare compunere mesaj, diagnostics, controale). Integrare SIP/MSRP
Ladder cu etichete dedicate. Stores (MsrpSessionStore,
MsrpDiagnosticsStore, MsrpTransactionStore) cu limite/clear/snapshot/
signals/thread-safety. Export JSON/TXT extins cu msrpSessions/msrpEvents
și câmpuri de transport messaging, documentând decizia schemaVersion.
Securitate/robustețe (limite dimensiune, protecție împotriva flood-ului,
redactare path-uri, fără logare credențiale). Teste unitare extensive
(SDP, path, frame parser/serializer, chunking, tranzacții, payload,
fallback, TLS, export) + regresii W090–W099. Test de integrare local
(harness MSRP peste loopback, port dinamic). Test manual documentat cu
SIP-Server-RTT (placeholders). Documentație: msrp-foundation.md,
msrp-transport.md, msrp-protocol.md, msrp-security.md, msrp-testing.md +
actualizări msrp-diagnostics.md/sip-message.md/imdn.md/is-composing.md/
windows-trace-json-export.md/project-status.md. Versioning conform
politicii W090–W099, fără release/merge.

## Commituri recomandate

`feat(msrp): add session and negotiation models`;
`feat(msrp): implement msrp sdp offer answer`;
`feat(msrp): add path parser and session lifecycle`;
`feat(msrp): implement frame parser and serializer`;
`feat(msrp): implement tcp and tls transports`;
`feat(msrp): add send report and transaction handling`;
`feat(msrp): implement chunking and byte range assembly`;
`feat(msrp): dispatch cpim imdn and is-composing payloads`;
`feat(messaging): add msrp transport policy and sip fallback`;
`feat(ui): add msrp session and diagnostics pages`;
`feat(export): include msrp sessions and protocol events`;
`test(msrp): add protocol and transport validation`;
`docs(msrp): document msrp foundation workflow`.

## Criterii de acceptare (rezumat)

Rezultat minim acceptat: negociere SDP funcțională; TCP transport
funcțional; TLS implementat sau limitarea justificată clar și izolată;
parser/serializer incremental; SEND; response; REPORT; Message-ID;
transaction-id; Byte-Range; chunking; text/plain; CPIM; IMDN;
is-composing; session lifecycle; fallback SIP MESSAGE; UI/diagnostics/
export; teste automate; harness local; zero regresii.

## Raport final obligatoriu (42 puncte)

branch; branch de pornire; versiune veche/nouă; schema JSON veche/nouă;
fișiere modificate/adăugate; decizia arhitecturală MSRP; suportul PJSIP
reutilizat și ce a fost implementat separat; modelul sesiunii; negocierea
SDP implementată; setup active/passive/actpass/holdconn; TCP transport;
TLS transport și validarea certificatelor; parserul MSRP; serializerul
MSRP; SEND și răspunsuri; REPORT și statusuri; transaction-id și
Message-ID; Byte-Range și chunking; continuation +/$/#; payload-uri
suportate; integrarea CPIM; integrarea IMDN; integrarea is-composing;
politica MSRP/SIP MESSAGE fallback; integrarea cu apelurile audio/video/
RTT; UI adăugat; integrarea în ladder; diagnostics/stores; export
JSON/TXT; configurarea introdusă; limitele de securitate; teste unitare
rulate; teste de integrare/harness; test manual cu SIP-Server-RTT; ce a
fost validat real; ce NU a putut fi validat; ce rămâne pentru W101 LMPE
Interoperability; limitări cunoscute; commituri; git status; push status;
confirmare fără merge în main/release.
