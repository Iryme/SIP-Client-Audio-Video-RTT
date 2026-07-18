# Tools Messaging Inspector — Current State (Task W113F audit)

Documents what `Tools → Messaging` (`MessagingDiagnosticsPage`) and
`Tools → MSRP` (`MsrpPage`) already show, as audited during Task W113F.
**Not extended in this task** — this is a snapshot of existing behavior
plus a gap list for a future follow-up, written so the "Client is simple,
Tools is complex" requirement's current baseline is on record.

## MessagingDiagnosticsPage — two independent feeds

1. **Send SIP MESSAGE composer** — an independent send capability (calls
   `SipManager::sendSipMessage()` directly), separate from the Client's
   `ClientMessagingController`.
2. **Message History table** (`m_historyTable`) — fed directly by
   `MessageHistoryStore` signals, the *same* store the Client reads (just
   unfiltered — every row, protocol events included, unlike the Client's
   `userVisibleHistoryFor()`). Has Direction/Content-Type filters, a
   Call-ID filter, and a "Mark as Read" action. `historyStatusText()`
   already special-cases `isImdnReport`/`isTypingNotification`/
   `deliveryState`/`outboundStatus` into readable icon-decorated text (✓✓
   Delivered, 👁 Displayed, ⚠ Failed, "IMDN report", "typing…", etc.) — the
   most correct existing precedent for how a protocol-event row should be
   *labeled* once you decide to show it.
3. **Raw trace table** (`m_table`) — fed by `MessagingEventStore`/
   `MessagingDiagnosticsStore`, sourced independently from raw SIP trace
   capture (`SipTraceLogger`/`PjsipTraceModule`), never from
   `MessageHistoryStore`. Columns today: **Time, Dir, Transport, From, To,
   Call-ID, Content-Type, Preview, Parse, Encoding**. Filters: Call-ID text
   filter, payload-kind filter (plain/html/cpim/imdn/is-composing/sdp/
   rcs-ft-http). Export: Text/JSON/Interop-JSON.

## MsrpPage

Transport/session-level detail: MSRP configuration, a manual test-session
group (experimental), a sessions table, a diagnostics table, and an MSRP
Relay Diagnostics (RFC 4976, experimental) table. Content/message-body
detail (CPIM/IMDN/is-composing parsing) is not duplicated here — that
lives in `MessagingDiagnosticsPage`'s raw trace table.

## Gap list vs. the full requested field set

Requested: direction, timestamp, conversation, peer, Call-ID, Message-ID,
transaction ID, selected/actual transport, fallback, Content-Type, parse
result, correlation status, delivery status, raw body, raw SIP, raw MSRP,
error, retry.

| Present | Where |
|---|---|
| Direction, timestamp, Call-ID, Content-Type | Both tables |
| Preview/parse result | Raw trace table (`Preview`, `Parse` columns) |
| Per-row transport | Raw trace table (`Transport` column) |
| Delivery-state icons | Message History table (`historyStatusText()`) |

| Missing as a column today | Notes |
|---|---|
| Conversation grouping | Neither table groups by conversation; both are flat, filterable by Call-ID/peer instead |
| Message-ID | Present only in `ClientMessagingView::appendHistoryRow()`'s tooltip, not as a table column anywhere |
| Transaction ID | Not surfaced in either table (exists on `MsrpTransaction`/`MsrpTransactionStore` internally) |
| Fallback reason | Exists as a field (`MessageHistoryEntry::fallbackReason`) but not a column in either table |
| Full raw SIP / raw MSRP text | Only a truncated "Preview" column; no dedicated full-text raw-body column |
| Error / retry | Not present as explicit columns |

## Recommendation (not done in this task)

A follow-up task should add: a Message-ID column (both tables), a
transaction-ID column (raw trace table, sourced from
`MsrpTransactionStore`), a fallback-reason column (Message History table,
straightforward since the field already exists on the entry), and either
a details-dialog or a wider raw-body column for full (not truncated) SIP/
MSRP text on demand. None of this blocks W113F's actual fix (protocol
payloads no longer leaking into the Client) — it's purely about closing
the remaining gap between "what Tools shows today" and "the full technical
field list the task spec asked for."
