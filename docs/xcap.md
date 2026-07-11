# XCAP Foundation (Task W099)

Foundation-level RFC 4825 XCAP client: an asynchronous HTTP GET/PUT/DELETE
(+optional HEAD) client against a configurable XCAP root, generic-XML
document validation before PUT, URL-redacted diagnostics, and an Interop
JSON export section. Resource Lists / policy semantics for any specific AUID
(pres-rules, resource-lists, rls-services, xcap-caps) are explicitly **not**
implemented in this task — every document is treated as opaque XML. MSRP
remains permanently disabled and untouched; Presence (Task W098), SIP
MESSAGE/IMDN/is-composing, and audio/video/RTT are all unaffected — XCAP is
plain HTTP and never touches SIP transport or the SIP Ladder's trace tap.

## Architecture

XCAP is plain HTTP, so it deliberately does **not** reuse the SIP raw-trace
pipeline (`SipTraceLogger`/`PjsipTraceModule`) that Presence/Messaging
diagnostics are built on — there is no SIP message to capture. Instead:

1. **`XcapClient`** (`src/sip/XcapClient.h/.cpp`) — singleton wrapping a
   `QNetworkAccessManager`. `get()`/`put()`/`del()`/`head()` each start one
   asynchronous request and return immediately; the result (success or
   failure) is delivered later via the `operationCompleted(XcapResult)`
   signal. The UI never touches `QNetworkReply`/`QNetworkAccessManager`
   directly — only `XcapResult`, mirroring the "UI never binds to
   low-level transport objects" rule established for `PresenceInfo` in
   Task W098.
2. **`XcapDiagnosticsStore`** (`src/sip/XcapDiagnosticsStore.h/.cpp`) —
   subscribes to `XcapClient::operationCompleted`, appends to a bounded
   list (`kMaxRetainedEntries = 500`, oldest evicted first), and re-emits
   `entryLogged`/`cleared` for the "XCAP Diagnostics" section of the XCAP
   page and for `InteropTraceExporter`'s new `xcapEvents` array.

`XcapRequestBuilder` (pure, no `QNetworkAccessManager`) and
`XcapAuthHeaderBuilder` (pure Basic-header construction) factor the
request-model logic out of `XcapClient`'s actual network execution, so both
are fully unit-testable without a live server — the same "separate the
callback → model mapping" approach used for pjsua2 callbacks in Task W098.

## 1. XCAP models

`src/sip/XcapModels.h` — plain Qt structs, no `QNetworkReply` type ever
escapes into them:

- **`XcapServerConfig`** — `rootUri`, `xui`, `username`, `authMode`
  (None/Basic/Digest), `validateXmlBeforePut`, `timeoutSeconds`,
  `verifyTls`. The password is never a field here — see §3.
- **`XcapDocument`** — `auid`, `xui` (overrides the server config's `xui`
  when non-empty; empty ⇒ a "global" document), `documentName` (default
  `"index"`), `nodeSelector` (optional). `buildUri(rootUri)` constructs the
  full RFC 4825 document URI:
  `{root}/{auid}/{users/<xui>|global}/{documentName}[~~nodeSelector]`.
- **`XcapOperation`** — one outbound request before execution (method,
  document, root URI, PUT body, timestamp).
- **`XcapResult`** — the only XCAP type the UI binds to: method, root URI,
  AUID, XUI, document selector, node selector, redacted URL, HTTP
  status/reason, Content-Type, Content-Length, ETag, Last-Modified,
  timestamp, request duration (ms), parse status (n/a/ok/partial/error),
  warnings, a size-capped body preview, and a network-error flag +
  message.

## 2. XCAP client — GET/PUT/DELETE/HEAD

All four operations go through `XcapClient::execute()`, which builds a
`QNetworkRequest` via `XcapRequestBuilder::build()`, applies the configured
transfer timeout (`QNetworkRequest::setTransferTimeout`) and TLS
verification mode, attaches credentials per §3, and issues the request via
`QNetworkAccessManager::get/put/deleteResource/head`. The result is always
delivered asynchronously through `QNetworkReply::finished` → `onReplyFinished()`
→ `operationCompleted` — the calling thread (UI) is never blocked waiting
for a reply.

`PUT` additionally validates the XML body **synchronously** before ever
opening a connection when `XcapServerConfig::validateXmlBeforePut` is true
(default): an invalid document short-circuits directly to an
`operationCompleted` emission with `parseStatus = Error` and no network
request is ever sent.

`HEAD` is implemented (the task marked it optional "if the infrastructure
allows" — `QNetworkAccessManager::head()` makes it trivial to support).

## 3. Authentication

- **None** (default) — no `Authorization` header is ever attached, even if
  a username/password happen to be configured.
- **Basic** — the header is built proactively (no discovery round-trip)
  via `XcapAuthHeaderBuilder::basicAuthorizationHeader()`, a pure function
  unit-tested in isolation.
- **Digest** — handled by Qt's own `QNetworkAccessManager::authenticationRequired`
  challenge/response machinery: `XcapClient`'s constructor connects a
  handler that supplies the stored username/password **only** when the
  reply came from a request whose `authMode == Digest` (tracked via a
  dynamic `QObject` property set at request time, cleared once the reply
  is deleted) — a server never receives Digest credentials for a request
  that was configured as None/Basic. No manual digest hashing is
  implemented in this client; Qt Network's built-in handling is trusted
  for the actual RFC 7616 computation, consistent with the task's
  "no fragile manual transactions" spirit.
- **OAuth is not implemented** (explicitly out of scope).

Passwords are **never** stored in `AppSettings`/the INI file and never
logged. They go through the existing `CredentialStore` secure-backend
mechanism (Windows Credential Manager in production, the same class SIP
profile passwords use), keyed by the fixed pseudo-profile id `"xcap"` +
the configured username — independent of which SIP profile is currently
active, since an XCAP server is not necessarily tied to a specific SIP
account. The XCAP page's password field is always blank on load; typing a
new value and leaving the field stores it immediately, an empty field
means "keep the existing stored password."

## 4. XML validation

`src/sip/XcapXmlValidator.h/.cpp` — `QXmlStreamReader`-based, no DOM:

- **Rejected**: an empty document (unless the caller explicitly allows
  it — GET response classification does, PUT validation never does), any
  `<!DOCTYPE` declaration (checked before the document is ever handed to
  the XML reader — defense in depth against external entity expansion,
  on top of `QXmlStreamReader` itself never resolving external
  entities/DTDs), an XML declaration with an `encoding=` value outside a
  small known-safe whitelist (`utf-8`/`utf-16`/`us-ascii`/`iso-8859-1`),
  a syntax error, or an unbalanced/missing root element.
- **Accepted**: well-formed XML with a single balanced root element and a
  known (or absent) encoding declaration.

The same validator classifies GET response bodies for diagnostics
(`parseStatus`/`warnings` in `XcapResult`) when the response's
`Content-Type` contains `xml` — with `allowEmpty = true`, since an empty
XCAP document is a valid (if unusual) GET result, not a validation
failure.

## 5. AUID readiness

The AUID combo box on the XCAP page is pre-populated with `resource-lists`,
`pres-rules`, `rls-services`, and `xcap-caps` (free-text editable for any
other AUID), and every `XcapResult`/export event carries the AUID used —
but **no AUID-specific schema, semantics, or validation is implemented**.
Every document, regardless of AUID, is handled identically as generic XML.
This is intentional (task requirement 5) — full Resource Lists (`<list>`/
`<entry>` semantics) and pres-rules policy documents are deferred to a
future task.

## 6. XCAP Diagnostics

Surfaced as part of the "XCAP" nav page (not a separate page — the task
allows this as long as it is a "clearly separate zone," which the
dedicated XCAP page already is, distinct from Call Control/Presence): an
operation log table (Method, URL (redacted), HTTP Status, Content-Type,
Duration (ms), ETag, Last-Modified, Parse status, Timestamp) fed live from
`XcapDiagnosticsStore::entryLogged`, plus a document viewer showing the
last GET result / the PUT body being composed. **Credentials are never
shown** — the log table has no username/password column, and
`XcapResult` itself has no password field to accidentally display.

## 7. URL redaction

`src/sip/XcapUrlRedactor.h/.cpp` (independent of `UrlRedactor`, which is
tuned for RCS download URLs, not XCAP selectors): keeps scheme, host, and
port verbatim; keeps up to the first three path segments verbatim
(typically AUID + `users`/`global` + XUI-or-document-name — enough to be
useful in diagnostics); folds any remaining path segments **and the
entire query string** (which may carry a sensitive node selector or an
auth token) into a short SHA-256-based fingerprint. Userinfo
(`user:pass@host`) is never included, since the redactor only ever reads
`QUrl::host()`/`port()`, never `userInfo()`.

## 8. JSON export — `xcapEvents`

`InteropTraceExporter::exportToJson()` gained a new top-level `xcapEvents`
array (purely additive — `kSchemaVersion` stays **2**), built from
`XcapDiagnosticsStore`'s entries, independent of both `events` and
`presenceEvents`. Each event: `eventId`, `method`, `timestamp`,
`duration`, `urlRedacted`, `auid`, `xui`, `selector`, `nodeSelector`,
`status`, `contentType`, `contentLength`, `etag`, `lastModified`,
`parseStatus`, `warnings`, `networkError`, and (when present)
`errorString`.

## 9. Config (`AppSettings`, `xcap/` key prefix)

| Setting | Default | Notes |
|---|---|---|
| `enableXcap` | `false` | Master toggle; gates Test Connection/GET/PUT/DELETE |
| `xcapRoot` | `""` | Never assumed; must be set from the UI |
| `xcapXui` | `""` | Default XUI for per-user documents |
| `xcapUsername` | `""` | Password stored separately via `CredentialStore` |
| `xcapAuthentication` | `"none"` | `"none"` / `"basic"` / `"digest"` |
| `validateXmlBeforePut` | `true` | Client-side XML validation gate for PUT |
| `xcapTimeout` | `15` (seconds) | Per-request transfer timeout |
| `xcapVerifyTls` | `true` | `false` disables TLS peer verification (lab/self-signed servers only) |

## 10. UI — "XCAP" page

A dedicated top-level nav page (`src/gui/panels/XcapPage.h/.cpp`, `NavRail`
entry `"xcap"`), separate from Call Control and Presence per the task's
explicit requirement. Sections: server configuration (Enable XCAP, Root
URI, XUI, Auth mode, Username/Password, Validate XML, Timeout, Verify TLS,
Test Connection); document operation (AUID, XUI override, Document name,
Node selector, GET/PUT/DELETE buttons); a document viewer/editor (GET
result display, PUT body input) plus an operation-result label; and the
operation log table described in §6.

**Test Connection** performs a GET against `xcap-caps/global/index` — a
reasonable, harmless read used purely to validate reachability/auth/TLS
configuration.

## 11. Ladder / Diagnostics integration

Per the task's explicit requirement, XCAP HTTP operations are **not**
drawn as SIP traffic — `SipLadderWidget`/`SipTraceLogger` are completely
untouched by this task, and no XCAP operation ever appears as a row in the
SIP Ladder. The only integration points are the XCAP Diagnostics log
(§6) and the Interop JSON export's `xcapEvents` array (§8), both entirely
independent of the SIP-trace-based Messaging/Presence diagnostics
pipelines.

## 12. What is NOT implemented

- Resource Lists semantics (`<list>`/`<entry>` parsing/editing),
  pres-rules policy semantics, rls-services semantics — every AUID is
  generic XML (explicitly out of scope, see §5).
- OAuth authentication.
- A configurable per-SUBSCRIBE... n/a (XCAP has no SUBSCRIBE; not
  applicable to this task).
- Automatic conditional PUT (`If-Match`/`ETag`-based optimistic
  concurrency) — `ETag`/`Last-Modified` are captured and displayed in
  diagnostics, but no conditional-request logic is built on top of them
  yet.
- XCAP Diff / auto-refresh / change notifications.
- Digest auth's actual challenge/response cryptography is not
  hand-implemented — it relies entirely on Qt Network's built-in support
  (see §3); this client has no independent digest implementation to unit
  test beyond confirming credentials are only attached for `Digest`-mode
  requests.
- MSRP is untouched and remains fully disabled.

## 13. Manual interoperability test (SIP-Server-RTT)

Placeholders only — never a real server/user:

1. Open the XCAP page, enable "Enable XCAP".
2. Set Root URI to `<xcap-root>` and XUI to `<xui>` (e.g.
   `sip:<user>@<server-host>`); pick an Authentication mode and enter
   credentials if the server requires them.
3. Click **Test Connection** and confirm a `200 OK` (or another expected
   status) appears in the operation log with a redacted URL.
4. Set AUID to `xcap-caps`, Document to `index`, click **GET** — confirm
   the document viewer shows the server's capabilities document and the
   log row shows `parseStatus = ok`.
5. Set AUID to `resource-lists`, Document to `index`, click **GET** —
   confirm the current resource-lists document (or a `404`/empty-document
   result if none exists yet) appears, with the correct HTTP status in the
   log.
6. Type a minimal well-formed `<resource-lists>` XML document into the
   viewer/editor and click **PUT** — confirm a `201 Created` or `200 OK`
   appears, with an `ETag` captured in the log.
7. Click **GET** again on the same document — confirm the just-PUT content
   is returned and its `ETag` matches.
8. Click **DELETE** — confirm the document is removed (subsequent GET
   returns `404`).
9. Export the Interop JSON (from the Messaging Diagnostics page's export
   action, which now also includes `xcapEvents`) and confirm all of the
   above operations appear under `xcapEvents` with the expected `method`/
   `status`/`auid`/`parseStatus` values and a redacted `urlRedacted` (no
   credentials, no full query string).

This scenario was documented but not executed against a live
SIP-Server-RTT instance in this session (no server was available) — see
the result report for confirmation.
