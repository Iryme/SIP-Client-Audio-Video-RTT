# SIP Presence (Task W098)

Foundation-level RFC 3856/3863 SIP Presence support: SUBSCRIBE/NOTIFY
subscription lifecycle, PIDF parsing, a per-entity presence store, SIP
Ladder/Interop JSON diagnostics, and an experimental own-status PUBLISH
control. XCAP and resource lists were explicitly out of scope for this task
— an XCAP client + diagnostics foundation now exists (Task W099, see
[xcap.md](xcap.md)), but it treats every document as generic XML and has no
Presence-specific integration (e.g. pres-rules policy semantics remain
unimplemented). MSRP remains permanently disabled and untouched.

## Architecture — two independent pipelines

Mirroring the disjoint-pipeline pattern already used for Message History vs.
Messaging Diagnostics (see `docs/message-history.md`), Presence uses **two
independent producers**, deliberately never cross-wired, so no two writers
ever race to update the same state:

1. **Live subscription pipeline** (`SipAccount` → `SipManager` →
   `PresenceStore`). A long-lived pjsua2 `Buddy` (owned by `SipAccount`,
   `subscribe=true`) drives the actual SUBSCRIBE/NOTIFY exchange.
   `Buddy::onBuddyState()` reports the current `BuddyInfo`
   (contact/status/activity/note/subscription-state/termination-reason) via
   the `buddyPresenceChanged` signal (queued onto the Qt thread, same
   pattern as every other pjsua2 callback in this codebase). `SipManager`
   maps that into a `PresenceInfo` and calls `PresenceStore::upsert()`. This
   is the **only** writer to `PresenceStore` — it is the authoritative
   current-state for the Presence page's subscription table and for
   auto-resubscribe decisions.

2. **Diagnostics pipeline** (`SipTraceLogger` → `PresenceDiagnosticsStore` →
   `InteropTraceExporter`/`SipLadderWidget`). Every raw SUBSCRIBE/NOTIFY
   message already captured by the existing, unmodified raw-trace tap
   (`PjsipTraceModule`, Task W090) is independently re-examined: Event /
   Subscription-State / Expires headers are extracted, and a
   `application/pidf+xml` body is parsed via `PidfParser` into a
   `PresenceTraceEntry`. This never writes into `PresenceStore` — it only
   feeds the SIP Ladder (which already displays it for free, see below) and
   `InteropTraceExporter`'s new `presenceEvents` JSON section.

Neither pipeline touches `MessageHistoryStore` or `MessagingEventStore`:
Presence is not introduced into Message History as an ordinary message, per
the task's explicit requirement.

## 1. PresenceInfo model

`src/sip/PresenceInfo.h` — pure Qt/text struct, never holds a pjsua2 type.
Fields: `entityUri`, `contactUri`, `basicStatus` (open/closed/unknown),
`extendedStatus` (available/away/busy/do-not-disturb/on-the-phone/
offline/unknown), `note`, `tupleId`, `priority`, `timestamp`, `expires`,
`subscriptionState` (pending/active/terminated/unknown),
`subscriptionReason`, `contentType`, `parseStatus` (ok/partial/error),
`parseWarnings`.

`PresenceInfo::normalizeSubscriptionReason()` maps any raw reason token
(from a `Subscription-State: terminated;reason=...` header or pjsua2's
`BuddyInfo::subTermReason`) onto one of RFC 3265's well-known reasons
(timeout/deactivated/probation/rejected/noresource/giveup/invariant), or
`unknown` if unrecognized.

## 2. PIDF parser

`src/sip/PidfParser.h/.cpp` parses `application/pidf+xml` via
`QXmlStreamReader`, matching elements by **local name only** (namespace
prefix/URI ignored), tolerant of element order and unknown fields. Parses
`presence@entity`, the first `<tuple>`'s `id`, `status/basic`, `contact` +
its `priority` attribute, `note` (presence- or tuple-level), and
`timestamp`. Multiple `<tuple>` elements are tolerated — only the first is
taken as canonical (documented limitation; see below).

**Result classification**: `ParseStatus::Error` for an empty body, a body
exceeding `PidfParser::kMaxPidfBytes` (64 KiB — Task W098 requirement 14's
size cap; the body is never even handed to the XML parser), a missing
`<presence>` root, or an XML syntax error. `ParseStatus::Partial` for a
missing `entity` attribute or a missing `tuple/status/basic`. Otherwise
`ParseStatus::Ok`.

**Safety**: `QXmlStreamReader` never resolves external entities/DTDs and
never performs network access — this alone satisfies "no external entity
expansion, no network access." No contact/entity URI is ever executed or
auto-opened by any code in this task.

## 3. Extended status (RPID/CIPID) recognition

`PidfParser` recognizes the element local names `away`, `busy`,
`on-the-phone`, `do-not-disturb`/`dnd`, `offline` anywhere in the document
(regardless of namespace prefix or nesting) and maps them to
`PresenceInfo::ExtendedStatus`. This is intentionally **not** a full
RPID/CIPID implementation — unrecognized extension elements are silently
ignored (no fatal error), and the extended status falls back to one implied
by the basic status (open → available, closed → offline) when no
recognized activity element was found.

The live-subscription pipeline's extended-status mapping is even more
limited: pjsua2's `PresenceStatus::activity` (`pjrpid_activity`) only
distinguishes `AWAY` and `BUSY` — `do-not-disturb`/`on-the-phone` are only
ever surfaced via the diagnostics pipeline's raw PIDF parsing, not via the
live Buddy list.

## 4. SUBSCRIBE

`SipManager::subscribePresence(targetUri, error)` — gated by
`AppSettings::enablePresence()` **and** `enablePresenceSubscribe()` (both
default OFF). Delegates to `SipAccount::subscribePresence()`, which creates
a long-lived `pj::Buddy` (`BuddyConfig{ uri, subscribe=true }`) owned by
`SipAccount::Impl` for the life of the subscription (per pjsua2's own
requirement — the library does not track Buddy instances itself; only the
owning instance's destructor unregisters it). Uses `Event: presence`,
`Accept: application/pidf+xml` — both built internally by pjsua2, never
hardcoded by this client beyond the target URI (always UI/config-supplied).

**Expires is not currently sent per-subscribe**: pjsua2's `BuddyConfig` has
no `expires` field — the SUBSCRIBE's `Expires` value is entirely managed by
pjsip's evsub module using its own internal default, not by anything this
client sets per-request. `AppSettings::presenceDefaultExpiresSeconds`
(default 300 s) is stored, surfaced in the UI's "Expires" field, and passed
in to future export/documentation, but there is currently no pjsua2 API to
apply it to the actual outbound SUBSCRIBE — this is a known limitation (see
§18), deliberately not worked around with a raw `pjsip_evsub`/manual
transaction per the task's explicit "no fragile manual SIP transactions"
rule.

## 5. NOTIFY receive

There is no separate "dedicated NOTIFY callback" at the pjsua2 layer beyond
`Buddy::onBuddyState()` (state changes) — instead, the **diagnostics**
pipeline gives full-fidelity NOTIFY inspection: `PresenceDiagnosticsStore`
extracts `Event`, `Subscription-State` (state + `reason=`/`expires=`
params), `Content-Type`, body, `Call-ID`, `CSeq`, `From`, `To`, and
`timestamp` directly from the already-captured raw SIP text (the same
`SipTraceLogger` feed the SIP Ladder uses) — see `PresenceDiagnosticsStore::
buildEntry()`. A body-less NOTIFY (valid per RFC 3265, e.g. the final NOTIFY
on termination) is not treated as a parse error.

Content-Type validation: only `application/pidf+xml` bodies are handed to
`PidfParser`; anything else is left unparsed (`entry.pidf` stays
default/`Ok` with an empty entity — the raw trace is still captured and
still appears in the ladder/export, just without structured PIDF fields).

## 6. Subscription lifecycle & auto-resubscribe

States: pending / active / terminated / unknown (`PresenceInfo::
SubscriptionState`, mapped from pjsua2's `pjsip_evsub_state` in
`SipAccount`'s `onBuddyState()`: `SENT`/`ACCEPTED`/`PENDING` → pending,
`ACTIVE` → active, `TERMINATED` → terminated, else unknown).

`PresenceResubscribePolicy` (`src/sip/PresenceResubscribePolicy.h`, pure
logic, zero PJSIP dependency, fully unit-tested):
- `shouldAutoRetry(reason)` — **false** for `rejected`/`noresource` (an
  explicit "no" from the peer or a nonexistent resource; retrying
  automatically would just hammer the server — the task's explicit rule),
  **true** for every other normalized reason (timeout/deactivated/
  probation/giveup/invariant/unknown).
- `backoffMs(attempt)` — exponential backoff, base 5 s, doubling per
  attempt, capped at 300 s (`kMaxBackoffMs`) — never a tight resubscribe
  loop.

`SipManager::onAccountBuddyPresenceChanged()` applies this policy: on
`terminated`, if Presence + Subscribe + Auto-resubscribe are all enabled and
the reason is retryable, `schedulePresenceResubscribe()` starts a per-entity
`QTimer` (single-shot) that calls `subscribePresence()` again when it fires;
the attempt counter resets to 0 the moment the subscription goes `active`
again. An explicit "Unsubscribe" from the UI cancels any pending backoff
timer for that entity.

## 7. PresenceStore

`src/sip/PresenceStore.h/.cpp` — singleton, `QHash<QString, PresenceInfo>`
current-state map (upsert by `entityUri`) plus a bounded
`QList<PresenceInfo>` history (`AppSettings::presenceMaxRetainedEvents`,
default 500, oldest evicted first). Mutex-protected for thread safety;
`presenceUpdated`/`cleared` Qt signals for UI consumption. Deliberately
**not** merged with `MessageHistoryStore` (different producer, different
shape — current-state-per-entity vs. append-only conversation log).

## 8. UI — "Presence" page

A dedicated top-level nav page (`src/gui/panels/PresencePage.h/.cpp`,
`NavRail` entry `"presence"`), separate from both Call control and
Messaging Diagnostics per the task's explicit requirement. Provides:
feature toggles (Enable Presence / Enable Subscribe / Enable Publish
(experimental) / Auto-resubscribe — all reflecting `AppSettings` and OFF by
default except auto-resubscribe, which only matters once the other two are
on); a target-SIP-URI field + configurable Expires + Subscribe/Unsubscribe/
Refresh buttons; an own-status selector (Available/Away/Busy/Do Not
Disturb/Offline) + "Set" button, clearly labeled experimental; and a table
(entity, basic state, extended state, note, subscription state, expires,
last update, status/error) fed live from `PresenceStore::presenceUpdated`.

## 9. Publish — experimental

`SipAccount::setOwnPresenceState()` calls pjsua2's
`Account::setOnlineStatus()`, which always updates this account's local
`PresenceStatus` (used when answering incoming SUBSCRIBEs, which pjsua2
auto-accepts by default — see below) regardless of Publish being enabled.
**Actually sending a PUBLISH request to the server** additionally requires
`AccountConfig.presConfig.publishEnabled = true`, which pjsua2 only reads at
**account-creation time** (`SipAccount::startRegistration()` sets it from
`AppSettings::enablePresencePublish()`) — toggling the "Enable Publish"
checkbox does **not** take effect until the active profile is
re-registered. This is why Publish is marked experimental in the UI rather
than a fully dynamic setting: it depends on (a) the server actually
supporting PUBLISH (many SIP servers/proxies do not), and (b) a
re-registration to pick up the setting. No manual/raw PUBLISH transaction is
built by this client — only pjsua2's own built-in mechanism is used, per the
task's "no fragile manual SIP transactions" rule.

**Incoming SUBSCRIBE** (someone watching *this* account) is handled by
pjsua2's own default `Account::onIncomingSubscribe()` behavior — left
un-overridden, since the default already auto-accepts (its `prm.code`
defaults to 200) and the NOTIFY body it sends reflects whatever
`setOwnPresenceState()` last set. No additional code was needed for this
foundation task.

## 10. SIP Ladder integration

`SipLadderWidget::contentTypeTag()` now tags `application/pidf+xml` bodies
as `[PIDF]`; `colorForTrace()` gives SUBSCRIBE/NOTIFY their own colors
(`#4FC3E8`/`#4FE8B0`). No further ladder changes were needed:
SUBSCRIBE/NOTIFY requests and responses already appear correctly (raw
capture is fully method-agnostic, `SipRawMessageParser` already derives
`method` generically from the request-line/CSeq), and raw SIP redaction
(Authorization/Proxy-Authorization → `[REDACTED]`) is likewise
transport-agnostic — zero changes needed there.

## 11. Diagnostic separation

Per the task's explicit requirement, Presence is **not** funneled into
`MessageHistoryStore`/`MessagingEventStore`/Messaging Diagnostics. The raw
ladder comes from `SipTraceLogger` (unchanged); Presence state/history comes
from `PresenceStore`; diagnostic export comes from
`PresenceDiagnosticsStore` → `InteropTraceExporter`. PIDF parsing happens
in exactly one place (`PidfParser`) and is never re-implemented in the UI.

## 12. JSON export — `presenceEvents`

`InteropTraceExporter::exportToJson()` gained a new top-level
`presenceEvents` array (purely additive — `kSchemaVersion` stays **2**),
built from `PresenceDiagnosticsStore`'s entries, independent of the
existing `events` array. Each event: `eventId`, `timestamp`, `direction`,
`method`, `callId`, `cseq`, `from`, `to`, `eventPackage`,
`subscriptionState`, `subscriptionExpires`, `subscriptionReason`,
`contentType`, `parseStatus`, `parseWarnings`, `rawSipRedacted`, and a
nested `presence` object: `entity`, `tupleId`, `basicStatus`,
`extendedStatus`, `contact`, `priority`, `note`, `timestamp` — present
(possibly empty) on every event.

## 13. Config (`AppSettings`, `presence/` key prefix)

| Setting | Default | Notes |
|---|---|---|
| `enablePresence` | `false` | Master toggle |
| `enablePresenceSubscribe` | `false` | Gates Subscribe/Unsubscribe/Refresh |
| `enablePresencePublish` | `false` | Experimental; needs re-registration |
| `presenceDefaultExpiresSeconds` | `300` | Shown in UI; not yet wired into the outbound SUBSCRIBE (see §4) |
| `presenceAutoResubscribe` | `true` | Only acts once Presence+Subscribe are on |
| `presenceMaxRetainedEvents` | `500` | `PresenceStore` history cap |
| `presenceDefaultState` | `"available"` | Last-selected own-status |

## 14. What is NOT implemented

- XCAP, resource lists (explicitly out of scope — Task W099).
- Full RPID/CIPID activity coverage (only away/busy/on-the-phone/
  do-not-disturb/offline element-name recognition).
- A configurable per-SUBSCRIBE `Expires` value actually reaching the wire
  (pjsua2's `BuddyConfig` has no such field — see §4).
- A dynamically-toggleable Publish setting (requires re-registration — §9).
- Presence-aware contact list integration (`ContactsPanel`) — the Presence
  page's table is independent of the existing Contacts panel.
- MSRP is untouched and remains fully disabled.

## 15. Manual interoperability test (SIP-Server-RTT)

Placeholders only — never a real server/user:

1. Register a profile (`<profile-name>`) against SIP-Server-RTT
   (`<server-host>`).
2. Open the Presence page, enable "Enable Presence" and "Enable Subscribe".
3. Enter a target `sip:<user>@<server-host>` and click Subscribe.
4. Confirm the server responds (200 OK to SUBSCRIBE, visible in the SIP
   Ladder as a blue `[PIDF]`-tagged SUBSCRIBE row followed by the response).
5. Confirm a NOTIFY arrives (green row, `Subscription-State: active`) and
   that the Presence page's table updates with the entity's basic/extended
   state.
6. Toggle the watched user's status on the server side (if possible) and
   confirm a follow-up NOTIFY updates the table.
7. Export the Interop JSON (Messaging Diagnostics page's export, or the
   equivalent Presence export action) and confirm the SUBSCRIBE/NOTIFY pair
   appears under `presenceEvents` with the expected `subscriptionState`/
   `presence.basicStatus`.
8. Click Unsubscribe; confirm a final NOTIFY with
   `Subscription-State: terminated` arrives and the table's subscription
   state updates accordingly.

This scenario was documented but not executed against a live
SIP-Server-RTT instance in this session (no server was available) — see the
result report for confirmation.
