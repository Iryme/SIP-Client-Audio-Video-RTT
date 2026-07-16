# Tools Navigation (Task W111)

Consolidates every technical/diagnostic/test page that was previously a
top-level nav item into a single `Tools` entry, so the primary nav rail
shows only end-user product surfaces.

## Before / after

| Before (11 nav items) | After (5 nav items) |
|---|---|
| Dashboard, Clients, SIP Ladder, Messaging, Presence, XCAP, MSRP, Call History, Logs, Diagnostics, Settings | Dashboard, Clients, Call History, **Tools**, Settings |

SIP Ladder, Messaging (diagnostics), Presence, XCAP, MSRP, Logs, and
Diagnostics (the "Diagnostics Center" hub) moved unchanged into `ToolsPage`
(`src/gui/panels/ToolsPage.{h,cpp}`) as `QTabWidget` sub-tabs. No page's
internal implementation changed — this was a pure relocation.

## Lazy construction

Sub-tabs follow the same lazy-build pattern `MainWindow::ensurePage()`
already used for top-level pages (`m_pageBuilt[index]` guard) one level
down: `ToolsPage::ensureSubTab(int index)` + `m_subTabBuilt[]`. Opening
Tools only builds the SIP Ladder sub-tab (the default) immediately; the
other six build on first visit. `kPageCount` in `MainWindow` dropped from 11
to 5 accordingly.

## Deep links

Old top-level nav ids (`sipladder`, `messaging`, `presence`, `xcap`, `msrp`,
`logs`, `diagnostics`) still work as `MainWindow::onNavPageRequested()`
arguments — they now route to page index 2 (Tools) plus the matching
sub-tab via `ToolsPage::openSubTab()`, instead of being rejected as unknown.
This kept existing deep-link emitters working with **no changes on their
end**:

- `DiagnosticsCenterPanel::openSipLadderRequested`/`openLogsRequested` (now
  wired inside `ToolsPage` itself, since it owns `DiagnosticsCenterPanel`)
- Dashboard shortcut cards (`DashboardShortcutCard`, ids `sipladder`/`logs`)

`CallHistoryPanel::redialRequested` is unaffected — it targets `clients`,
which never moved.

## Persistence

`AppSettings::toolsLastSubTab()`/`setToolsLastSubTab()` remembers the
last-visited sub-tab across app runs (default: `sipladder`), so reopening
Tools returns to where the user left off.

## Automation IDs

`ToolsPage`'s `QTabWidget` has `objectName` `toolsTabs`. The individual
wrapped pages (`SipLadderPage`, `MessagingDiagnosticsPage`, `PresencePage`,
`XcapPage`, `MsrpPage`, `DiagnosticsPanel`, `DiagnosticsCenterPanel`)
deliberately keep their own pre-existing `objectName`s (e.g.
`"DiagnosticsPanel"`, used by an existing QSS selector in
`ThemeManager.cpp`) — renaming them to `toolsSipLadder`/`toolsLogs`/etc.
as originally sketched in the task spec would have silently broken that
styling. Automation targeting a specific sub-tab should go through
`toolsTabs`'s tab index/text instead.

`NavRail` buttons keep `objectName` `"NavButton"` for all five (an existing
`#NavButton` QSS selector styles checked/hover state); a stable per-button
id is exposed via `accessibleName` instead (`navDashboard`, `navClients`,
`navHistory`, `navTools`, `navSettings`) — see `NavRail::addNavButton()`.

## Not present in this codebase

The task's suggested Tools structure also listed **Packet Capture** and a
generic **Exports**/**Interop**/**Test-Developer** sub-tab. None of these
exist as distinct pages in this codebase (confirmed via inventory before
this task started) — no placeholder/empty tab was created for them, per
the task's own "don't invent tabs" rule. The closest existing equivalents
(SIP Ladder for message-level packet inspection, Diagnostics Center for
export/bundle generation) are already included.
