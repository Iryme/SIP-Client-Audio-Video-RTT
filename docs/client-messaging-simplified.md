# Client Messaging Simplification (Task W113F)

The Client Messaging view (`src/gui/panels/messaging/ClientMessagingView.{h,cpp}`)
is the end-user-facing conversational surface embedded in the Clients
page. This task addressed two reported problems: protocol XML leaking into
the conversation (see [messaging-content-type-routing.md](messaging-content-type-routing.md)
for that fix) and the view itself being too technical for normal use.

## What stays in the main view

- Contact/conversation selection (unchanged — `ConversationWorkspacePanel`).
- Presence + typing indicator row.
- History list — now only real user messages (protocol events filtered,
  see the routing doc).
- Composer: text input, Send, Send File (Experimental), Save Received
  File.
- Transport-outcome/session-status labels (brief, human-readable text,
  unchanged).

## What moved behind "Messaging options" (Task W113F)

Previously an always-visible `QGroupBox` ("Send options") sat between the
presence row and the history list, permanently showing three protocol-level
controls. It's now a `QToolButton` disclosure labeled "Messaging options",
collapsed by default (`AppSettings::clientMessagingAdvancedOptionsExpanded()`,
same persistence pattern as `CallWorkspacePanel`'s "Advanced diagnostics"
disclosure from Task W113a):

- **Transport selector** (Automatic / SIP MESSAGE / MSRP Preferred / MSRP
  Required) — unchanged options, just hidden by default. Automatic remains
  the default value.
- **Content-type selector** — **CPIM removed** as a manually selectable
  option. CPIM wrapping is applied automatically via the existing global
  `AppSettings::enableCpim()` toggle (Settings page), which
  `ClientMessagingController` already reads independently of this
  selector — exposing "CPIM" here as if it were a content type a user
  picks per-message never actually controlled CPIM wrapping and only
  invited confusion. Only **Plain text** and **HTML** remain.
- **Delivery receipts** (renamed from "Request IMDN:") — the same two
  checkboxes (delivered/read), just relabeled in plain language and moved
  into the disclosure.

A saved `clientMessagingContentType` setting that previously stored "CPIM"
(no longer a valid combo item) falls back safely to "Plain text" — the
combo's `findData()` returns -1 for a value with no matching item, and the
setup code clamps that to index 0.

## LMPE

LMPE never appeared in this view's transport or content-type selectors —
confirmed clean before this task and unchanged. Its unavailable status is
now enforced project-wide; see [lmpe-disabled-status.md](lmpe-disabled-status.md).

## What's NOT changed in this task

- No new UserMessage/ProtocolEvent enum was introduced across the codebase.
  The existing `MessageHistoryEntry` boolean flags
  (`isImdnReport`/`isTypingNotification`/the new `isUnsupportedOrMalformed`)
  plus the new `isProtocolEvent()` helper achieve the same practical
  outcome (protocol rows excluded from the Client's bubble list) with a
  much smaller, lower-risk change than a model-wide type rewrite would
  have been.
- HTML sanitization (Faza 14 of the task spec) was not audited or changed
  in this task — `text/html` rendering behavior is unchanged. Flagged as a
  follow-up, not silently assumed safe.
- The Tools → Messaging inspector's column set was not expanded (no new
  Message-ID/transaction-ID/fallback columns) — see
  [tools-messaging-inspector.md](tools-messaging-inspector.md) for what
  exists today and what a follow-up task would need to add.
- No manual two-peer GUI test was run (no live SIP peer available this
  session) — see the agent-result report for what's NOT RUN.
