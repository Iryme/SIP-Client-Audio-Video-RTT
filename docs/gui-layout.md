# GUI Layout

## Window Structure

```
┌──────────────────────────────────────────────────────────────────┐
│ File  View  Contacts  Calls  Messaging  Tools  Help              │ ← QMenuBar
├────┬───────────┬─────────────────────────────┬───────────────────┤
│    │           │  [Remote Name]     State Dur │  RTT  │  LMPE    │
│    │ Account   │  [Remote URI]                │ ──────────────── │
│    │ Card      │  [Mute][Video][Hold][Hangup] │ Remote typing:   │
│ N  │ ──────── │ ──────────────────────────── │ ──────────────── │
│ a  │ Search   │                              │ Transcript:      │
│ v  │ ──────── │   Video Area (16:9)          │                  │
│    │ Contacts │   [remote video placeholder] │                  │
│ R  │ list     │                    ┌────────┐│                  │
│ a  │          │     ● AUDIO        │ Local  ││                  │
│ i  │          │                    │Preview ││ ──────────────── │
│ l  │          │  Remote Video      └────────┘│ Input:  [Send]   │
│    │ ──────── │ ──────────────────────────── │                  │
│    │ +Contact │ [Call Info] [Media] [Stats]  │                  │
│    │ +Account │   (info tabs)                │                  │
├────┴───────────┴─────────────────────────────┴───────────────────┤
│ Diagnostics / Logs  INFO WARN ERR DEBUG RAW  Filter [Clear]...   │
│ ┌──────────┬──────┬──────────┬─────────────────────┬──────────┐ │
│ │ Time     │Level │ Category │ Message             │ Payload  │ │
│ └──────────┴──────┴──────────┴─────────────────────┴──────────┘ │
├──────────────────────────────────────────────────────────────────┤
│ ● Disconnected │ No account │ Transport: — │ IP: — │ Jitter: — │ │ ← StatusBar
└──────────────────────────────────────────────────────────────────┘
```

## Panel Dimensions

| Panel | Normal desktop (1440×900) | Minimum (1024×768) |
|---|---|---|
| Navigation rail | 64 px fixed | 64 px fixed |
| Sidebar | 260 px (resizable min 220 px) | 220 px |
| Center area | Flexible, min 420 px | 420 px |
| Right RTT/LMPE | 380 px (resizable min 300 px) | Collapses to tab |
| Diagnostics | 240 px height (resizable min 160 px) | 160 px |
| Status bar | 28 px fixed | 28 px fixed |

## Layout Stability Rules

1. No floating windows for core functions.
2. Layout only changes through controlled QSplitter handles.
3. Call controls (mute, video, hold, hangup) are always visible.
4. Log panel never pushes call controls out of view.
5. Video area maintains aspect ratio (paintEvent handles proportional scaling).
6. Local preview is anchored to video panel bottom-right, repositioned on resize.
7. RTT/LMPE panel remains docked; at minimum width it becomes a tab.
8. Splitter states are saved to QSettings and restored on launch.

## Profile Editor Dialog

`SipProfileEditorDialog` (`src/gui/dialogs/SipProfileEditorDialog.h`) is a modal dialog opened from `SidebarPanel`.

- Opens at 500×640 px with a scrollable body.
- Sections: General, SIP, Network, Transport, Extensions, Security, Advanced (collapsed by default).
- Advanced section is shown/hidden by a flat toggle button with arrow indicator.
- Password fields use `QLineEdit::Password` echo mode; a Show/Hide button toggles both fields at once.
- Password strength is shown as a colour-coded label: Weak (<8), Fair (8–11), Strong (12+).
- OK button triggers internal validation; a `QMessageBox::warning` is shown on error.
- Cancel closes without saving — no partial writes occur.
- Delete confirmation uses `QMessageBox::question` before calling `SipProfileManager::remove()`.

## Implementation Notes

- `QSplitter::setChildrenCollapsible(false)` prevents accidental collapse.
- `setMinimumWidth()` / `setMinimumHeight()` enforce minimum panel sizes.
- `VideoPanel::resizeEvent()` repositions all overlays on resize.
- The outer vertical splitter separates main content from diagnostics.
- The inner horizontal splitter separates sidebar | center | right panel.
- NavRail is outside the splitter, fixed at 64 px.
