# GUI Layout Specification

> **This document is the single source of truth for all GUI layout decisions.**
> All implementation must conform to this specification. Any proposed deviation
> must be recorded as a new ADR in `docs/architecture-decisions.md` before
> being implemented.

---

## Application Identity

- **Name:** SIP Client — Audio / Video / RTT
- **Type:** Generic cross-platform SIP multimedia softphone
- **Primary target:** Dark professional desktop application
- **Minimum usable resolution:** 1024 × 768
- **Primary design target:** 1440 × 900

---

## Top-Level Layout Structure

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  Menu bar  (40 px fixed height)                                              │
├──────┬─────────────┬──────────────────────────────────┬──────────────────────┤
│  Nav │   Sidebar   │         Main / Call area          │   Right RTT/LMPE     │
│  64px│  260px      │         (flexible, min 420px)     │   380px              │
│ fixed│  min 220px  │                                   │   min 320px          │
│      │             │                                   │                      │
│  (full height)     │                                   │                      │
├──────┴─────────────┴──────────────────────────────────┴──────────────────────┤
│  Diagnostics / Logs panel  (250 px default height, min 160 px)               │
├──────────────────────────────────────────────────────────────────────────────┤
│  Status bar  (30 px fixed height)                                            │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Panel Dimensions

### Normal desktop — 1440 × 900

| Region | Dimension | Type |
|---|---|---|
| Menu bar | 40 px height | Fixed |
| Navigation rail | 64 px width | Fixed |
| Account/contact sidebar | 260 px width | Resizable (min 220 px) |
| Main call/video area | ~700 px width | Flexible |
| Right RTT/LMPE panel | 380 px width | Resizable (min 320 px) |
| Diagnostics/logs panel | 250 px height | Resizable (min 160 px) |
| Status bar | 30 px height | Fixed |

### Minimum — 1024 × 768

| Region | Dimension | Behavior |
|---|---|---|
| Menu bar | 40 px height | Unchanged |
| Navigation rail | 64 px width | Always visible |
| Account/contact sidebar | 220 px (compact) | May switch to icon mode |
| Main call/video area | 420 px width | Minimum enforced |
| Right RTT/LMPE panel | Collapsed to tab | Docked tab, not floating |
| Diagnostics/logs panel | 160 px height (min) | Remains docked at bottom |
| Status bar | 30 px height | Unchanged |

---

## Region 1 — Top Menu Bar

**Height:** 40 px, fixed.

Menus:

| Menu | Purpose |
|---|---|
| File | New call, exit |
| View | Show/hide panels, full screen |
| Contacts | Add, import contacts |
| Calls | Call history, redial |
| Messaging | New RTT session, new LMPE message |
| Tools | Preferences, SIP accounts, debug bundle |
| Help | About |

---

## Region 2 — Left Navigation Rail

**Width:** 64 px, fixed. Never hidden. Never participates in any splitter.

Navigation items (top to bottom):

| Item | Page |
|---|---|
| Accounts | Account/profile management |
| Contacts | Contact list |
| Calls | Active/recent calls |
| Messages | RTT/LMPE sessions |
| History | Call history log |
| Dialpad | Numeric keypad dialer |
| Settings | Application preferences |

The Settings item is anchored at the bottom of the rail, separated by a spacer from the main navigation group.

Each item is a checkable `QToolButton` with `setAutoExclusive(true)`. Only one item is active at a time.

---

## Region 3 — Account / Contact Sidebar

**Default width:** 260 px. **Minimum:** 220 px.
Participates in the horizontal `QSplitter`. `setChildrenCollapsible(false)`.

### Account Card (top section)

Displays the currently active SIP profile:

| Field | Notes |
|---|---|
| Profile name | Bold, 13 px |
| SIP URI | `sip:user@domain`, muted color |
| Registration status | ● Registered (green) / ● Registering (yellow) / ● Not registered (red) |
| Registration duration | Time since registration, or expiry countdown |

### Contact List (main section)

- Search field (full width)
- Scrollable contact list:
  - Contact name
  - SIP URI
  - Presence / status placeholder
- Double-click to initiate call

### Action Buttons (bottom)

- **+ Add Contact** — opens add contact dialog
- **+ Add Account** — opens add SIP profile dialog

---

## Region 4 — Main Call / Video Area

**Minimum width:** 420 px. Flexible (takes remaining horizontal space).

### 4a — Call Header

**Height:** fixed, approximately 110 px.

Contents:

| Element | Notes |
|---|---|
| Remote name | Large, bold, 16 px |
| Remote SIP URI | Muted, 11 px |
| Call state | e.g. "Ringing", "In Call", "Held", "Idle" |
| Call duration | `hh:mm:ss`, monospace |
| Call control buttons | See below |

#### Call Control Buttons

All buttons must always be visible and reachable:

| Button | Checkable | Notes |
|---|---|---|
| Mute | Yes | Toggles microphone |
| Video | Yes | Toggles local camera |
| Share | No | Screen share — placeholder, disabled |
| Hold | Yes | Holds/resumes call |
| Keypad | Yes | Toggles DTMF keypad |
| Record | No | Call recording — placeholder, disabled |
| **Hangup** | No | Red, right-aligned, always visible |

The Hangup button is visually distinct (red background) and must never be obscured by other elements.

### 4b — Video Area

Occupies the flexible portion of the center column between the call header and the info tabs.

#### Remote Video

- Full area filled with remote video stream.
- Dark placeholder background with centered text when no video.
- Maintains aspect ratio — pillarbox or letterbox as needed.

#### Local Preview (Picture-in-Picture)

- **Position:** Anchored to bottom-right corner of the video area.
- **Size:** 160 × 120 px (approximately 20% of the video panel width, or a fixed PiP size).
- **Margin:** 12 px from right and bottom edges.
- **Must never move** except when the video panel itself is resized — repositioned in `resizeEvent()`.
- Displayed with a thin border and a "Local" label.
- No drag-to-move behavior in the initial implementation.

#### Overlay Indicators

- **Signal/media indicator** — top-right corner of video area (e.g. "● AUDIO", "● VIDEO").
- **Remote label** — bottom-left corner, shows remote participant name.
- All overlays repositioned in `resizeEvent()`.

### 4c — Info Tabs (below video)

**Height:** approximately 200 px, fixed (not part of video splitter).

Tabs:

#### Call Info tab

| Field | |
|---|---|
| Call ID | SIP Call-ID header |
| Remote URI | Full remote URI |
| Direction | Inbound / Outbound |
| State | Current call state |
| Start time | Call connect time |
| Audio codec | e.g. OPUS/48000 |
| Video codec | e.g. VP8, H.264, or "none" |
| Transport | UDP / TCP / TLS |
| Remote IP:port | RTP peer address |
| Security | SRTP / none |

#### Media tab

| Control | |
|---|---|
| Microphone selector | `QComboBox` — enumerates input devices |
| Speaker selector | `QComboBox` — enumerates output devices |
| Camera selector | `QComboBox` — enumerates video input devices |
| Video resolution selector | `QComboBox` — e.g. 720p, 480p |
| Audio level meters | Input and output VU meters |
| Device settings button | Opens OS device settings (platform-dependent) |

#### Statistics tab

| Metric | |
|---|---|
| Jitter | ms (RTP stream) |
| Packet loss | % (RTP stream) |
| RTT latency | ms (round-trip) |
| Audio bitrate | kbps |
| Video bitrate | kbps |
| RTP stream status | Active / Inactive / Error |

---

## Region 5 — Right RTT / LMPE Panel

**Default width:** 380 px. **Minimum:** 320 px.
Participates in the horizontal `QSplitter`. `setChildrenCollapsible(false)`.

At minimum window width (1024 px), this panel collapses into a docked tab beside the Call Info tabs rather than appearing as a separate side panel. It must **never** become a floating window.

### RTT Tab — Real-Time Text

| Element | Notes |
|---|---|
| RTT session state | e.g. "RTT: Inactive", "RTT: Active" |
| Remote typing area | Read-only, shows characters being received in real time |
| Transcript | Read-only scrolling history of the exchange |
| Local input field | Full-width text input |
| Send button | Sends current input, appends to transcript |
| Clear transcript button | Clears transcript display only |

RTT input sends on Enter key or Send button click. Each keystroke will eventually be transmitted live (RFC 4103); for the skeleton, full message send is sufficient.

### LMPE Tab — Messages

| Element | Notes |
|---|---|
| LMPE session state | e.g. "LMPE: Inactive" |
| Message history list | Scrollable list of sent/received messages |
| Local input field | Full-width text input |
| Send button | Sends message |
| Delivery/status placeholder | Per-message status icon (future) |

---

## Region 6 — Diagnostics / Logs Panel

**Default height:** 250 px. **Minimum:** 160 px.
Participates in the vertical `QSplitter` (bottom section). `setChildrenCollapsible(false)`.

### Toolbar

#### Log Level Toggles

| Level | Default | Notes |
|---|---|---|
| INFO | **ON** | Normal events |
| WARN | **ON** | Recoverable issues |
| ERROR | **ON** | Failures |
| DEBUG | OFF | Developer tracing |
| RAW | OFF | Full SIP/RTP payloads — requires explicit activation |

RAW must never be turned on by a default state, saved state restore, or keyboard shortcut without explicit user interaction. A confirmation prompt is recommended when enabling RAW.

#### Category Filter

Multi-select filter — all enabled by default:

`APP` `SIP` `SDP` `MEDIA` `RTT` `LMPE` `ETSI` `PLATFORM`

#### Action Buttons

| Button | Action |
|---|---|
| Search | Filter log table by text (real-time) |
| Clear | Remove all displayed rows |
| Copy Selected | Copy selected rows to clipboard as tab-separated text |
| Export Visible | Save displayed rows to a `.txt` file |
| Export Debug Bundle | Package logs + SIP traces into a zip — passwords/secrets excluded |

### Log Table Columns

| Column | Width | Notes |
|---|---|---|
| Time | 100 px | `hh:mm:ss.zzz` |
| Level | 55 px | INFO / WARN / ERROR / DEBUG / RAW |
| Category | 70 px | APP / SIP / etc. |
| Message | Stretch | Primary log text |
| Payload | 180 px | Optional raw data (SIP/SDP/T.140) |

Color coding by level:

| Level | Color |
|---|---|
| INFO | `#50b8e0` (blue) |
| WARN | `#e0b850` (yellow) |
| ERROR | `#e05050` (red) |
| DEBUG | `#888888` (grey) |
| RAW | `#606060` (dark grey) |

---

## Region 7 — Status Bar

**Height:** 30 px, fixed. Built from `QStatusBar` with permanent widgets.

Fields (left to right, separated by vertical dividers):

| Field | Example |
|---|---|
| Global connection state | ● Registered / ● Disconnected |
| Active account | `sip:alice@example.com` |
| SIP transport | `TLS` / `UDP` / `TCP` |
| Local IP | `192.168.1.10` |
| Jitter | `Jitter: 4 ms` |
| Packet loss | `Loss: 0.1%` |
| RTT latency | `RTT: 22 ms` |

All fields update in real time during an active call. When idle, jitter/loss/RTT show `—`.

---

## Critical Layout Stability Rules

These rules are non-negotiable and must be enforced in every implementation:

1. **No floating windows for core functions.** All panels are permanently docked.
2. **Layout changes only through controlled splitter handles.** No panels jump on state change.
3. **Call controls are always visible.** Mute, Video, Hold, Keypad, Hangup must never be scrolled out of view, overlapped, or hidden.
4. **Log console never pushes call controls out of view.** The vertical splitter separates diagnostics from the main content area.
5. **Video area keeps aspect ratio.** Implemented in `paintEvent()` or via Qt Multimedia video surface constraints.
6. **Local preview stays anchored.** Position is recalculated in `resizeEvent()`, not in a layout manager.
7. **RTT/LMPE panel stays docked.** At minimum width it becomes a tab adjacent to Call Info, not a floating window.
8. **Diagnostics stays docked at the bottom.** Minimum height enforced. Never floats.
9. **Splitter positions are persisted** via `QSettings` and restored on next launch — but only after the base layout is confirmed stable.
10. **`QSplitter::setChildrenCollapsible(false)`** is set on all splitters to prevent accidental collapse.
11. **Minimum sizes are enforced** via `setMinimumWidth()` / `setMinimumHeight()` on every panel.

---

## Qt Implementation Mapping

| Layout concept | Qt mechanism |
|---|---|
| Fixed nav rail | `setFixedWidth(64)` — outside splitter |
| Horizontal panel split | `QSplitter(Qt::Horizontal)` |
| Vertical main/diagnostics split | `QSplitter(Qt::Vertical)` |
| Stable minimum sizes | `setMinimumWidth()` / `setMinimumHeight()` |
| No collapse | `QSplitter::setChildrenCollapsible(false)` |
| Layout persistence | `QSplitter::saveState()` / `restoreState()` via `AppSettings` |
| Video overlay positioning | `resizeEvent()` with manual `move()` calls |
| Menu bar height | `QMenuBar` — height controlled by `setFixedHeight(40)` or stylesheet |
| Status bar height | `QStatusBar::setFixedHeight(30)` |

---

## Checklist for GUI Compliance

When implementing or reviewing any GUI change, verify:

- [ ] Window resizes cleanly from 1024×768 to full screen without layout break
- [ ] Hangup button is always visible and reachable
- [ ] Mute, Video, Hold buttons are always visible
- [ ] Diagnostics panel stays at the bottom
- [ ] No panel floats or detaches
- [ ] Splitter handles are visible and functional
- [ ] Local PiP preview stays at bottom-right of video area after resize
- [ ] Status bar remains at 30 px and does not grow
- [ ] RTT/LMPE panel is docked (right or tab) at all window widths
- [ ] Log panel level filter buttons show correct default state (INFO/WARN/ERROR on)
