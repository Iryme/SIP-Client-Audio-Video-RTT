# Application Settings

## Overview

`ApplicationSettings` is the canonical, typed settings service for all persistent application state. It wraps `QSettings` (INI format, user scope) and provides safe typed accessors with documented defaults.

**Location:** `src/settings/ApplicationSettings.h` / `src/settings/ApplicationSettings.cpp`

**Singleton access:**
```cpp
ApplicationSettings &s = ApplicationSettings::instance();
bool debugOn = s.diagLevelEnabled(LogLevel::Debug);
s.setDiagLevelEnabled(LogLevel::Debug, true);
```

---

## Storage

| Property | Value |
|---|---|
| Format | INI (`QSettings::IniFormat`) |
| Scope | User (`QSettings::UserScope`) |
| Organization | `SIPClient` |
| Application | `SIPClient` |
| Platform path (Windows) | `%APPDATA%\SIPClient\SIPClient\SIPClient.ini` |
| Platform path (Linux) | `~/.config/SIPClient/SIPClient.ini` |

---

## Settings Keys and Defaults

### Diagnostics level toggles

| Key | Type | Default | Description |
|---|---|---|---|
| `diagnostics/level/info` | bool | `true` | Show INFO entries |
| `diagnostics/level/warn` | bool | `true` | Show WARN entries |
| `diagnostics/level/error` | bool | `true` | Show ERROR entries |
| `diagnostics/level/debug` | bool | `false` | Show DEBUG entries |
| `diagnostics/level/raw` | bool | `false` | Show RAW SIP/SDP entries — **never enabled by accident** |

### Diagnostics UI state

| Key | Type | Default | Description |
|---|---|---|---|
| `diagnostics/category_filter` | string | `""` | Active category filter in the panel |

### Window layout

| Key | Type | Default | Description |
|---|---|---|---|
| `ui/geometry` | bytes | *(empty)* | Main window geometry (QMainWindow::saveGeometry) |
| `ui/state` | bytes | *(empty)* | Main window state (QMainWindow::saveState) |
| `ui/splitter/horizontal` | bytes | *(empty)* | Horizontal splitter positions |
| `ui/splitter/vertical` | bytes | *(empty)* | Vertical splitter positions |
| `ui/theme` | string | `"dark"` | Selected theme name |

### Placeholders (filled by later tasks)

| Key | Type | Default | Description |
|---|---|---|---|
| `sip/profile_id` | string | `""` | Active SIP profile ID |
| `media/microphone_id` | string | `""` | Selected microphone device ID |
| `media/speaker_id` | string | `""` | Selected speaker device ID |
| `media/camera_id` | string | `""` | Selected camera device ID |

---

## Security — Credential Storage

**Passwords and secrets are never stored in `ApplicationSettings`.**

SIP account passwords will be stored separately using the OS credential store:
- **Windows:** Windows Credential Manager (`wincred`)
- **Linux:** libsecret / KWallet

See [ADR-009](architecture-decisions.md#adr-009) for the decision record and [ADR-006](architecture-decisions.md#adr-006) for the original no-plain-text-password constraint.

---

## API Reference

```cpp
// Diagnostics
bool   diagLevelEnabled(LogLevel level) const;
void   setDiagLevelEnabled(LogLevel level, bool enabled);
QString diagCategoryFilter() const;
void   setDiagCategoryFilter(const QString &filter);

// Window layout
QByteArray windowGeometry() const;
void       setWindowGeometry(const QByteArray &geom);
QByteArray windowState() const;
void       setWindowState(const QByteArray &state);
QByteArray splitterState(const QString &key) const;
void       setSplitterState(const QString &key, const QByteArray &state);

// Theme
QString theme() const;
void    setTheme(const QString &theme);

// Placeholders
QString selectedSipProfileId() const;
void    setSelectedSipProfileId(const QString &id);
QString selectedMicrophoneId() const;
void    setSelectedMicrophoneId(const QString &id);
QString selectedSpeakerId() const;
void    setSelectedSpeakerId(const QString &id);
QString selectedCameraId() const;
void    setSelectedCameraId(const QString &id);

// Lifecycle
void resetToDefaults();
void sync();
```

---

## Reset to Defaults

```cpp
ApplicationSettings::instance().resetToDefaults();
```

Clears all `diagnostics/`, `ui/`, `sip/`, and `media/` keys, then syncs. After reset:
- INFO / WARN / ERROR levels are enabled (recovered from defaults).
- DEBUG and RAW levels are disabled.
- Theme reverts to `"dark"`.
- All device and profile IDs are cleared.

---

## Test Isolation

Tests must not use the production singleton. Construct a fresh instance with a unique org/app name:

```cpp
ApplicationSettings s("IrymeTest", "SIPClientTest_MyFeature");
// Use s directly — isolated from real user settings.
```

See `tests/test_application_settings.cpp` for full examples.

---

## Backward-Compatible Shim

`src/core/AppSettings.h` is a thin delegation shim that routes the original static API to `ApplicationSettings::instance()`. Existing callers (`MainWindow`) continue to work without changes.

New code should use `ApplicationSettings` directly.
