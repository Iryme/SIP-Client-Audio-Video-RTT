# Task-W101 — Live MSRP SIP Integration (agent prompt, verbatim)

## Repository

SIP-Client-Audio-Video-RTT

Branch nou:

```text
feature/w101-live-msrp-sip-integration
```

Branch de pornire:

```text
feature/w100-msrp-foundation
```

---

## Context

W100 a implementat fundația MSRP:

- stack MSRP independent pe Qt
- parser/serializer MSRP
- SEND
- REPORT
- CPIM
- IMDN
- is-composing
- session store
- diagnostics
- export
- harness local

Limitarea principală rămasă:

SDP-ul MSRP este doar model intern.

`m=message` NU apare încă în INVITE/200 OK transmise efectiv.

Scopul W101 este conectarea completă a MSRP la dialogul SIP real.

**LMPE NU se implementează în acest task.**

LMPE va fi implementat într-un task separat după validarea formatului și interoperabilității.

---

## Reguli permanente

Respectă toate regulile proiectului deja existente.

În plus:

1. Nu modifica pjproject dacă există o soluție prin API-ul public.
2. Auditul API-urilor PJSIP trebuie făcut înainte de implementare.
3. Nu considera funcțională negocierea MSRP dacă doar modelul intern conține `m=message`. Trebuie demonstrat că apare efectiv în SIP transmis.
4. Nu folosi workaround-uri bazate doar pe IP / port / peer URI pentru asocierea sesiunilor.
5. Nu inventa formate protocolare.
6. Menține proiectul buildable după fiecare etapă.

---

## Obiectiv

Transformă implementarea MSRP din W100 într-o implementare complet funcțională legată de dialogul SIP.

Nu trebuie afectate: audio, video, RTT, REGISTER, SIP MESSAGE, CPIM, IMDN, is-composing, Presence, XCAP.

---

## Faza 1 — Audit PJSIP

Analizează API-urile disponibile (PJSUA2, PJSIP C API, SDP callbacks, INVITE generation, media negotiation). Determină metoda sigură pentru introducerea secțiunii MSRP în SDP. Documentează alegerea. Nu presupune că API-ul actual permite modificarea SDP.

## Faza 2 — SDP real

Oferta SIP trebuie să conțină efectiv `m=message` cu toate atributele MSRP necesare. Validează: INVITE, 200 OK, re-INVITE, UPDATE, port 0, multiple media sections. Audio/video/RTT trebuie să rămână intacte. Trebuie demonstrat prin SIP capture / raw SIP / teste automate. Nu este suficient modelul intern.

## Faza 3 — MSRP Listener

Listener-ul MSRP trebuie pornit înainte de ofertarea SDP. Verifică TCP, TLS, advertised host, advertised port, session-id, To-Path, From-Path. Fără valori hardcodate. Session-id trebuie să fie random, unic, fără informații personale, nereutilizat.

## Faza 4 — SIP ↔ MSRP Mapping

Mapping complet între dialogul SIP și sesiunea MSRP folosind SIP Call-ID, local tag, remote tag, media index, session-id, To-Path, From-Path, connection role — nu exclusiv IP/port. Trebuie permisă existența simultană a mai multor sesiuni MSRP către același peer fără coliziuni.

## Faza 5 — Composer Integration

Conectează MessagingTransportPolicy la composer. Trebuie suportate: Automatic, MSRP Preferred, MSRP Required, SIP MESSAGE Only. Nu trebuie să existe duplicate.

## Faza 6 — MSRP Live Transport

Fluxul: Composer → MSRP SEND → 200 MSRP → REPORT → Message History → Diagnostics → Export. Implementarea trebuie să fie funcțională end-to-end.

## Faza 7 — Fallback

Fallback real pentru: MSRP indisponibil, sesiune neconfirmată, socket închis, timeout, peer reject → SIP MESSAGE. Dacă MSRP devine din nou disponibil, transportul trebuie să poată reveni automat pe MSRP fără restartul apelului.

## Faza 8 — Diagnostics

UI trebuie să afișeze: selected transport, negotiated transport, actual transport, fallback used, fallback reason, session state, transaction state, REPORT status, IMDN status.

## Faza 9 — Export

Schema JSON devine `schemaVersion = 3`. Adaugă: transportDecision, msrpSession, peerAssociation, fallbackReason, negotiationState. Compatibilitatea cu exporturile anterioare trebuie păstrată.

---

## Teste automate

Rulează: build ENABLE_PJSIP=ON, toate testele W090–W100, testele noi W101.

Teste obligatorii: SDP (m=message în INVITE/answer, Content-Length corect, UPDATE, re-INVITE, port 0, multiple media), Session Mapping (match corect, stale, timeout, duplicate, path mismatch, role mismatch), Transport (toate modurile, fallback, recovery, fără duplicate), Regresii (audio, video, RTT, SIP MESSAGE, Presence, XCAP, IMDN, is-composing).

## Teste manuale

Dacă mediul este disponibil: SIP-Server-RTT, Blink. Linphone se testează doar pentru funcționalitățile pe care le suportă. Nu declara interoperabilitate MSRP cu Linphone dacă aceasta nu există. Rezultate strict PASS / FAIL / BLOCKED / NOT RUN.

## Documentație

Adaugă: docs/msrp-live-sdp-integration.md, docs/msrp-peer-association.md, docs/msrp-fallback.md. Actualizează: docs/msrp-foundation.md, docs/msrp-transport.md, docs/msrp-testing.md, docs/project-status.md, docs/agent-prompts/W101-live-msrp-sip-integration.md, docs/agent-results/W101-live-msrp-sip-integration-result.md.

## Git

Baseline înainte de orice modificare. La final: `git push -u origin feature/w101-live-msrp-sip-integration`. Nu face merge în main / release.

## Raport Final (29 puncte)

Branch nou; branch de pornire; versiune veche/nouă; schema JSON veche/nouă; fișiere modificate; metoda de integrare SDP; justificarea tehnică; dovada că `m=message` apare efectiv pe wire; impact asupra audio/video/RTT; dedicated vs attached SIP dialog; mapping SIP ↔ MSRP; asocierea conexiunilor MSRP; rezolvarea diagnosticului de peer-connection; transport policy conectat la composer; fallback SIP MESSAGE real; SEND → response → REPORT real; export JSON; teste automate; teste manuale; PASS; FAIL; BLOCKED; NOT RUN; probleme client; probleme server; limitări rămase; ce rămâne pentru W102; commiturile create; git status; push status; confirmarea explicită că NU s-a făcut merge în main sau release.
