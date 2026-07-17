# Clients Page Layout (Task W113a)

A usability follow-up requested directly after W113, before continuing the
W112–W117 roadmap to W114: the Clients page's 4-always-visible-column
splitter was too wide and columns 1/2 were too vertically dense to use
comfortably. This reorganizes it to 3 columns, folding two pairs of
rarely-simultaneously-needed panels into tabs.

## Before → after

| Column | Before | After |
|---|---|---|
| 1 | `ConversationWorkspacePanel` stacked on top of a "Call Control" group (target input + buttons), a 12-button numeric dialpad, and `ContactsPanel` — four things competing for one ~340px-wide column's height. | Shared target input + Clear/Backspace row, then a `QTabWidget` with **Conversations** and **Contacts** tabs. The numeric dialpad is gone — URIs/numbers are typed directly into the target input (unchanged keyboard behavior); it was redundant next to the conversation list and typed SIP URIs, and the single biggest contributor to this column's clutter. |
| 2 | `CallWorkspacePanel` (21 status cards, buttons, device combos, emergency section) + `VideoPanel`. | Same position; `CallWorkspacePanel` internally splits its cards into an always-visible essential row (8 cards) and a collapsed-by-default "Advanced diagnostics" disclosure (13 cards) — see [call-workspace.md](call-workspace.md). |
| 3 | `RttPanel` (own column). | Folded into column 3's `QTabWidget`, alongside Messaging. |
| 4 | `ClientMessagingView` (own column). | Folded into column 3's `QTabWidget` as the **Messaging** tab (default-active), with **RTT** as the second tab. |

Net: 4 columns → 3, and the two most internally-cluttered columns (1 and 2)
are each meaningfully thinner without losing any functionality — every
widget, signal connection, and behavior is unchanged, only *where* each
lives and how much is visible at once.

## Splitting Messaging/RTT into tabs doesn't change their behavior

Both `ClientMessagingView` and `RttPanel` keep receiving their live
`SipManager`/`RttSession`/`PresenceStore` signal updates while their tab
isn't the active one — Qt does not suspend a hidden widget's slots, it only
skips repainting it. Switching to the RTT tab mid-call shows fully
up-to-date state, not a stale snapshot.

## Splitter-state migration

`AppSettings::loadSplitterState("clients")` stores `QSplitter::saveState()`'s
raw `QByteArray`, which encodes (among other things) the widget count.
Existing users' saved 4-column state cannot be replayed onto the new
3-widget splitter — `QSplitter::restoreState()` returns `false` (and
leaves sizes untouched) on a widget-count mismatch, silently producing an
unproportioned layout if left unguarded. `MainWindow::buildClientsPage()`
now checks `restoreState()`'s return value and falls back to the new
default `setSizes({300, 700, 380})` whenever it fails — so an upgrading
user gets a fresh, correctly-proportioned 3-column layout once, instead of
a broken one that only fixes itself if they happen to manually resize a
column afterward.
