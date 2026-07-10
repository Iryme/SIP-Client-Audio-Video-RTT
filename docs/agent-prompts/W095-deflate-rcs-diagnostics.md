# Agent Prompt — Task-W095: Deflate Decoding and RCS Payload Diagnostics Hardening

Verbatim task prompt as received, preserved for provenance.

---

Task-W095 — Deflate Decoding and RCS Payload Diagnostics Hardening

Branch:
feature/w095-deflate-rcs-diagnostics

Pleacă din:
feature/w094-windows-trace-json-export

Nu pleca din main/release, deoarece W090–W094 nu sunt încă integrate.

Context real de interoperabilitate:
Testarea cu Linphone iOS a arătat:
- unele payload-uri message/imdn+xml cu Content-Encoding: deflate sunt decodate corect;
- altele produc:
  - "deflate decode failed"
  - "Content-Type declared message/imdn+xml but no IMDN disposition could be parsed."
- Linphone trimite și:
  application/vnd.gsma.rcs-ft-http+xml
- raw SIP poate conține payload binar comprimat și nu trebuie tratat direct ca text UTF-8.

Obiectiv:
Întărește pipeline-ul de decodare și diagnostic pentru payload-uri SIP MESSAGE comprimate și adaugă suport diagnostic minimal pentru RCS FT HTTP, fără să implementezi descărcare de fișiere, transfer media sau MSRP real.

Reguli:
- Nu hardcoda IP-uri, domenii, porturi, URI-uri sau credențiale.
- Nu rupe audio/video/RTT, SIP MESSAGE, CPIM, IMDN sau is-composing existent.
- Nu activa MSRP.
- Nu descărca automat fișiere din URL-uri primite.
- Nu executa și nu deschide automat payload-uri sau atașamente.
- Nu bloca UI thread.
- Parsing-ul XML trebuie să fie sigur și tolerant.
- Nu face merge în main.

Cerințe:

1. Audit Content-Encoding
Identifică exact unde este extras și decodat body-ul SIP MESSAGE.

Separă clar:
- raw body bytes;
- decoded body bytes;
- text body;
- body preview.

Nu converti payload-ul comprimat în QString înainte de decompresie.

2. Deflate decoder robust
Adaugă un decoder reutilizabil pentru Content-Encoding: deflate care să suporte controlat:
- zlib-wrapped deflate;
- raw deflate fallback;
- eventual gzip doar dacă header-ul sau semnătura indică explicit gzip.

Strategie recomandată:
- încearcă zlib wrapper;
- dacă eșuează cu format incompatibil, încearcă raw deflate;
- nu masca erorile reale;
- returnează rezultat structurat:
  - success
  - decoded bytes
  - encoding variant used
  - error code/message
  - input size
  - output size.

Adaugă limite configurabile/sigure:
- max compressed input;
- max decompressed output;
- max expansion ratio.

Protejează împotriva decompression bomb.

3. Binary-safe SIP body extraction
Verifică faptul că body-ul este extras după Content-Length în bytes, nu prin operații text care pot:
- opri la NULL;
- normaliza CRLF;
- modifica octeții;
- presupune UTF-8.

Dacă infrastructura actuală nu poate păstra body bytes exact:
- introdu un câmp QByteArray/raw bytes în modelul intern potrivit;
- păstrează compatibilitate cu API-ul text existent.

4. Content-Encoding metadata
Extinde MessagingTraceEntry/MessagingEvent/exportul cu:
- contentEncoding
- decodeStatus: not-needed/decoded/failed/unsupported/limit-exceeded
- decodeVariant: zlib/raw-deflate/gzip/none
- compressedBodyLength
- decodedBodyLength
- decodeError, doar când există.

Nu expune payload binar brut în preview text.

5. IMDN retry după decode
Pentru message/imdn+xml:
- parserul IMDN trebuie să ruleze doar pe body-ul decodat;
- dacă decompresia reușește, parsează:
  - message-id
  - delivered
  - displayed
  - failed
  - error
  - original-recipient
  - final-recipient.
- dacă decompresia eșuează, marchează Partial cu warning clar;
- nu încerca XML parsing pe bytes comprimați.

6. RCS FT HTTP diagnostic parser
Adaugă suport diagnostic read-only pentru:
application/vnd.gsma.rcs-ft-http+xml

Parsează minimal și sigur:
- file-info type
- file-size
- file-name
- content-type
- data URL
- expiration/until
- thumbnail metadata, dacă există
- disposition sau playing-length, dacă există.

Adaugă payloadType:
rcs-ft-http

Nu:
- descărca URL-ul;
- accesa rețeaua;
- salva automat fișierul;
- afișa imaginea automat.

7. URL safety/redaction
URL-ul de transfer poate conține token-uri sau identificatori temporari.

În UI și export:
- păstrează opțional URL-ul complet doar dacă politica actuală de diagnostic permite;
- implicit oferă și o versiune redacted:
  - scheme
  - host
  - port
  - path parțial sau hash;
- elimină query parameters/token-uri sensibile din preview.

Documentează decizia.

8. UI Messaging Diagnostics
Pentru payload comprimat afișează:
- Content-Encoding
- Decode status
- Decode variant
- Compressed size
- Decoded size
- Warning/error.

Pentru RCS FT HTTP afișează read-only:
- file name
- MIME type
- size
- expiration
- URL redacted.

Nu înghesui call control și nu adăuga download button în acest task.

9. Export JSON
Extinde InteropTraceExporter cu:
- contentEncoding
- decodeStatus
- decodeVariant
- compressedBodyLength
- decodedBodyLength
- decodeError, dacă există

Pentru RCS:
rcsFileTransfer {
  fileInfoType,
  fileName,
  fileSize,
  contentType,
  dataUrlRedacted,
  expiresAt,
  thumbnailPresent
}

Păstrează compatibilitatea cu schema W094:
- nu elimina și nu redenumi câmpuri existente;
- dacă este necesar, crește schemaVersion și documentează migrarea.

10. Fixtures din captura reală
Folosește exporturile reale furnizate doar ca sursă de fixture.

Creează fixture-uri sanitizate și minimale pentru:
- IMDN deflate zlib care se decodează;
- IMDN deflate raw;
- payload deflate invalid/trunchiat;
- output peste limită;
- RCS FT HTTP XML valid;
- RCS XML incomplet;
- URL cu query/token pentru testarea redaction.

Nu introduce în repo:
- IP-uri reale;
- domenii reale;
- credențiale;
- nonce-uri;
- URL-uri/token-uri reale;
- mesaje personale.

11. Tests
Adaugă teste pentru:
- zlib-wrapped deflate decode;
- raw deflate decode;
- invalid deflate;
- truncated deflate;
- output limit;
- expansion-ratio limit;
- body cu NULL bytes;
- Content-Length byte-accurate;
- IMDN delivered după decode;
- IMDN displayed după decode;
- invalid compressed IMDN => Partial;
- RCS FT HTTP parse complet;
- RCS FT HTTP parse parțial;
- URL redaction;
- JSON export fields;
- regresie pentru text/plain/CPIM/is-composing necomprimate.

Rulează build complet cu ENABLE_PJSIP=ON și întreaga suită CTest.

12. Docs obligatorii
Adaugă:
- docs/content-encoding-diagnostics.md
- docs/rcs-ft-http-diagnostics.md

Actualizează:
- docs/messaging-diagnostics.md
- docs/windows-trace-json-export.md
- docs/sip-message.md
- docs/project-status.md
- docs/agent-prompts/W095-deflate-rcs-diagnostics.md
- docs/agent-results/W095-deflate-rcs-diagnostics-result.md

13. Versioning
Respectă politica proiectului:
- stabilește dacă modificarea este patch sau minor;
- actualizează versiunea și changelog-ul/documentele de release conform structurii existente;
- nu crea release și nu face merge fără cerere explicită.

Commituri recomandate:
- fix(messaging): preserve binary sip message bodies
- fix(messaging): support zlib and raw deflate decoding
- feat(messaging): add rcs ft http diagnostics parser
- feat(export): include decode and rcs diagnostic metadata
- feat(ui): show compressed payload diagnostics
- test(messaging): add real-world deflate and rcs fixtures
- docs(messaging): document content encoding diagnostics

Flux obligatoriu:
1. Verifică:
   git status
   git branch
   git log --oneline -10

2. Pleacă din W094:
   git checkout feature/w094-windows-trace-json-export
   git pull --ff-only
   git checkout -b feature/w095-deflate-rcs-diagnostics

3. Inspectează implementarea existentă înainte de modificare.

4. Implementează incremental, cu commituri tematice.

5. Rulează:
   - build complet ENABLE_PJSIP=ON
   - ctest --output-on-failure

6. Actualizează documentația și versiunea.

7. Push:
   git push -u origin feature/w095-deflate-rcs-diagnostics

8. Nu face merge în main/release.

Raport final obligatoriu:
1. branch
2. branch de pornire
3. versiune veche/nouă
4. fișiere modificate/adăugate
5. cauza exactă a eșecurilor deflate
6. cum este păstrat body-ul binary-safe
7. variante deflate suportate
8. limitele de siguranță introduse
9. cum este reparată parsarea IMDN comprimată
10. ce parsează pentru RCS FT HTTP
11. cum sunt redactate URL-urile
12. ce s-a schimbat în exportul JSON/schemaVersion
13. ce rămâne diagnostic-only
14. ce NU este implementat
15. teste rulate și rezultat
16. limitări
17. commituri
18. git status
19. push status
