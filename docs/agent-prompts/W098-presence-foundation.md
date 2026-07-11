Lucrăm pe repo:
https://github.com/Iryme/SIP-Client-Audio-Video-RTT

Task-W098 — Presence Foundation

Branch:
feature/w098-presence-foundation

Pleacă din:
feature/w097-is-composing

Obiectiv:
Implementează suport de bază pentru SIP Presence folosind SUBSCRIBE, NOTIFY și PIDF, integrat în clientul Windows ca funcționalitate de test și interoperabilitate. Nu implementa încă XCAP; acesta rămâne pentru W099.

Reguli permanente:

* Nu hardcoda IP-uri, domenii, porturi, useri, parole sau URI-uri.
* Toate destinațiile și setările trebuie să provină din profil/config/UI.
* Nu rupe audio/video/RTT, SIP MESSAGE, IMDN sau is-composing.
* Nu activa MSRP.
* Nu implementa XCAP în acest task.
* Nu implementa resource lists în acest task.
* Tot ce este experimental trebuie să aibă flag UI/config.
* Nu bloca UI thread.
* Actualizează docs și project-status.
* Nu face merge în main/release.

Scop funcțional:
Clientul trebuie să poată:

* publica propria stare de prezență, dacă stack-ul PJSIP permite;
* subscrie la prezența unui SIP URI;
* primi și interpreta NOTIFY;
* afișa starea contactului;
* exporta diagnosticul Presence;
* apărea corect în SIP Ladder.

Cerințe:

1. Presence model

Adaugă un model intern PresenceInfo cu:

* entity URI
* contact URI
* basic status:
    * open
    * closed
    * unknown
* activity/status extins:
    * available
    * away
    * busy
    * do-not-disturb
    * offline
    * unknown
* note
* tuple id
* priority, dacă există
* timestamp
* expires
* subscription state
* subscription reason
* raw content type
* parse status
* parse warnings

Nu lega direct UI-ul de obiectele pjsua2.

2. PIDF parser

Adaugă parser sigur pentru:

* application/pidf+xml

Parsează minimal:

* presence entity
* tuple id
* status/basic
* contact
* contact priority
* note
* timestamp

Parserul trebuie:

* să accepte namespace-uri XML;
* să nu depindă de ordinea elementelor;
* să tolereze câmpuri necunoscute;
* să marcheze Partial pentru PIDF incomplet;
* să nu blocheze UI;
* să nu permită entity expansion extern sau acces la rețea.

3. Extensii Presence

Dacă payload-ul include elemente recognoscibile pentru:

* away
* busy
* do-not-disturb
* on-the-phone
* offline

mapează-le într-o stare extinsă internă.

Nu încerca suport complet pentru toate extensiile RPID/CIPID în acest task.

Dacă extensia nu este cunoscută:

* păstrează basic status;
* adaugă warning informativ, fără eroare fatală.

4. SUBSCRIBE

Adaugă suport pentru inițierea unei subscrieri Presence către un SIP URI configurabil.

Parametri:

* target SIP URI
* expires
* account/profile folosit
* enable/disable subscription

Folosește Event:
presence

Accept:
application/pidf+xml

Nu hardcoda durata.

Default recomandat:

* subscribe disabled implicit;
* expires configurabil, de exemplu 300 secunde;
* auto-resubscribe configurabil și activ doar dacă Presence este enabled.

5. NOTIFY receive

Adaugă callback dedicat pentru NOTIFY Presence.

Extrage:

* Event
* Subscription-State
* expires
* reason
* Content-Type
* body
* Call-ID
* CSeq
* From
* To
* timestamp
* account/profile

Validează:

* Event trebuie să fie presence;
* Content-Type trebuie să fie suportat;
* body-ul trebuie parsezat prin PIDF parser;
* NOTIFY fără body trebuie tratat conform stării subscription.

6. Subscription lifecycle

Suportă stările:

* pending
* active
* terminated
* unknown

Pentru terminated extrage reason:

* timeout
* deactivated
* probation
* rejected
* noresource
* giveup
* invariant
* unknown

Auto-resubscribe:

* nu reîncerca pentru rejected/noresource fără acțiune explicită;
* aplică backoff pentru timeout/probation;
* nu crea bucle rapide de SUBSCRIBE;
* nu bloca UI.

7. Presence publish

Dacă PJSIP/pjsua2 oferă suport stabil pentru PUBLISH/PRES:

* adaugă publish pentru propria stare.

Stări selectabile:

* Available
* Away
* Busy
* Do Not Disturb
* Offline

Dacă suportul PJSIP nu este suficient de clar sau stabil:

* implementează modelul și UI-ul;
* marchează Publish ca experimental/disabled;
* documentează exact limitarea;
* nu construi manual tranzacții SIP fragile doar pentru a bifa cerința.

8. Presence store

Adaugă PresenceStore:

* append/update by entity URI;
* current state per entity;
* history opțional limitat;
* clear;
* configurable max retained events;
* thread-safe sau explicit UI-thread confined;
* signals pentru update UI.

Nu amesteca PresenceStore cu MessageHistoryStore.

9. UI

Adaugă o pagină sau zonă clară:
Presence

Funcții:

* target SIP URI
* Subscribe
* Unsubscribe
* Refresh
* expires configurabil
* auto-resubscribe checkbox
* tabel contacte/subscriptions
* entity
* basic state
* extended state
* note
* subscription state
* expires
* last update
* status/error

Adaugă și control pentru propria stare doar dacă publish este implementat sau marcat experimental clar.

Nu înghesui call control și Messaging Diagnostics.

10. SIP Ladder

Asigură afișarea pentru:

* SUBSCRIBE outbound/inbound
* NOTIFY outbound/inbound
* răspunsuri SIP asociate
* badge/label Presence
* Content-Type application/pidf+xml
* Subscription-State
* Event: presence

Raw SIP trebuie să rămână redactat conform regulilor existente.

11. Messaging Diagnostics / diagnostic separation

Presence nu trebuie introdus forțat ca mesaj obișnuit în Message History.

Folosește:

* SipTraceLogger pentru raw ladder;
* PresenceStore pentru stare și istoric Presence;
* Interop export pentru diagnostic.

Nu duplica parsing-ul PIDF în UI.

12. Export JSON

Extinde InteropTraceExporter cu evenimente Presence sau o secțiune presenceEvents.

Câmpuri:

* eventId
* timestamp
* direction
* method
* callId
* cseq
* from
* to
* eventPackage
* subscriptionState
* subscriptionExpires
* subscriptionReason
* contentType
* parseStatus
* parseWarnings
* rawSipRedacted

PIDF:
presence {
entity,
tupleId,
basicStatus,
extendedStatus,
contact,
priority,
note,
timestamp
}

Păstrează schemaVersion 2 dacă modificarea este strict aditivă și compatibilă.
Crește schema doar dacă structura existentă nu poate fi extinsă compatibil.

13. Config/AppSettings

Adaugă setări:

* enablePresence
* enablePresenceSubscribe
* enablePresencePublish
* presenceDefaultExpiresSeconds
* presenceAutoResubscribe
* presenceMaxRetainedEvents
* presenceDefaultState

Default conservator:

* enablePresence = false sau controlat explicit din UI;
* subscribe = false;
* publish = false;
* auto-resubscribe = true doar după activarea Presence;
* fără target URI implicit.

14. Siguranță și thread model

* Callback-urile PJSIP nu trebuie să actualizeze direct widget-uri.
* Folosește signal/slot queued.
* Nu face parsing XML greu în UI thread dacă payload-ul poate fi mare.
* Limitează dimensiunea PIDF acceptată.
* Validează Content-Length în bytes.
* Nu procesa XML extern.
* Nu executa URL-uri/contact URI-uri.
* Nu deschide automat linkuri.

15. Tests

Adaugă teste pentru:

* PIDF open;
* PIDF closed;
* PIDF cu contact și priority;
* PIDF cu note;
* PIDF cu namespace;
* PIDF cu tuple multiple;
* PIDF incomplet;
* PIDF invalid XML;
* extended status away;
* extended status busy;
* extended status do-not-disturb;
* SUBSCRIBE request model;
* NOTIFY active;
* NOTIFY pending;
* NOTIFY terminated;
* terminated reason rejected;
* terminated reason timeout;
* expires parsing;
* store update by entity;
* duplicate NOTIFY handling;
* stale update handling;
* auto-resubscribe backoff;
* no retry pentru rejected/noresource;
* Presence JSON export;
* raw SIP redaction;
* regresii W090–W097.

Dacă există callback-uri PJSIP greu de unit-testat:

* separă mapping-ul callback → model;
* testează mapping-ul fără rețea;
* documentează testul manual pentru server.

16. Test manual interoperabilitate

Documentează un scenariu manual cu SIP-Server-RTT:

* client înregistrat;
* Presence enabled;
* SUBSCRIBE către un URI configurat;
* server răspunde;
* client primește NOTIFY;
* PIDF apare în UI;
* Subscription-State este actualizat;
* ladder conține SUBSCRIBE/NOTIFY;
* export JSON conține evenimentul.

Nu hardcoda utilizatori sau IP-uri în documentație.
Folosește placeholders:

* sip:<user>@<server-host>
* <profile-name>

17. Docs obligatorii

Adaugă:

* docs/presence.md

Actualizează:

* docs/sip-message.md, doar pentru delimitarea MESSAGE vs Presence;
* docs/windows-trace-json-export.md;
* docs/project-status.md;
* docs/agent-prompts/W098-presence-foundation.md;
* docs/agent-results/W098-presence-foundation-result.md.

Documentează clar:

* ce funcționează;
* ce este experimental;
* ce depinde de server;
* dacă PUBLISH este implementat sau doar pregătit;
* limitările RPID/CIPID;
* lifecycle SUBSCRIBE/NOTIFY;
* testarea manuală.

18. Versioning

Respectă politica proiectului:

* modificarea este probabil minoră deoarece adaugă o capabilitate nouă;
* identifică unde este urmărită versiunea reală a aplicației;
* actualizează versiunea și changelog-ul dacă politica repo-ului o cere;
* nu presupune că CMakeLists.txt este sursa versiunii;
* nu crea release;
* nu face merge.

Commituri recomandate:

* feat(presence): add presence model and pidf parser
* feat(presence): implement subscribe and notify lifecycle
* feat(presence): add presence store
* feat(ui): add presence page and subscription controls
* feat(export): include presence diagnostics
* test(presence): add pidf and subscription tests
* docs(presence): document presence workflow

Flux obligatoriu:

1. Verifică:
    git status
    git branch
    git log –oneline -12
2. Pleacă din W097:
    git checkout feature/w097-is-composing
    git pull –ff-only
    git checkout -b feature/w098-presence-foundation
3. Inspectează suportul Presence disponibil în versiunea PJSIP/pjsua2 folosită efectiv de proiect.
4. Implementează modelul și parserul înainte de UI.
5. Implementează lifecycle-ul SUBSCRIBE/NOTIFY.
6. Integrează UI și export.
7. Rulează:
    * build complet cu ENABLE_PJSIP=ON;
    * ctest –output-on-failure;
    * test manual documentat cu SIP-Server-RTT dacă serverul este disponibil.
8. Actualizează documentația și versiunea conform politicii repo-ului.
9. Push:
    git push -u origin feature/w098-presence-foundation
10. Nu face merge în main/release.

Raport final obligatoriu:

1. branch
2. branch de pornire
3. versiune veche/nouă și unde este urmărită
4. fișiere modificate/adăugate
5. ce parsează PIDF
6. cum funcționează SUBSCRIBE
7. cum funcționează NOTIFY
8. lifecycle și auto-resubscribe
9. cum funcționează PresenceStore
10. ce afișează UI-ul
11. dacă PUBLISH este implementat sau experimental
12. integrarea în SIP Ladder
13. modificările în exportul JSON
14. ce este configurabil
15. ce NU este implementat
16. teste automate rulate
17. test manual cu serverul
18. limitări
19. commituri
20. git status
21. push status
