# Secure Credential Storage

**Status:** IMPLEMENTED — Task 7

## Overview

`CredentialStore` is a singleton service that stores SIP account passwords and future secrets using the platform OS keychain. It deliberately provides no way to retrieve passwords in bulk or export them — only targeted, per-profile operations are exposed.

Passwords are **never** stored in QSettings INI files, SipProfile structs, application logs, or diagnostic export bundles.

---

## Architecture

```
CredentialStore (src/security/CredentialStore.h)
  │
  └── ICredentialBackend (interface)
        ├── WindowsCredentialBackend  — Windows Credential Manager (Advapi32)
        └── MemoryCredentialBackend   — in-memory, tests only, NOT secure
```

`CredentialStore::instance()` selects the appropriate backend at startup:

| Platform | Backend | isSecure() |
|---|---|---|
| Windows | `WindowsCredentialBackend` | true |
| Other | null — all operations return false | false |

Tests inject `MemoryCredentialBackend` via `CredentialStore::instance().setBackend(...)` before any test operation.

---

## API Reference

### CredentialStore

```cpp
// Singleton
static CredentialStore &instance();

// Query
QString backendName() const;
bool    isSecureBackendAvailable() const;

// Operations (never log password values)
bool    storePassword(const QString &profileId, const QString &username, const QString &password);
QString loadPassword (const QString &profileId, const QString &username, bool *found = nullptr);
bool    deletePassword(const QString &profileId, const QString &username);
bool    hasPassword  (const QString &profileId, const QString &username);

// Signals
void credentialStored(const QString &profileId, const QString &username);
void credentialLoaded(const QString &profileId, const QString &username);
void credentialDeleted(const QString &profileId, const QString &username);
void backendError(const QString &operation, const QString &error);

// Backend injection for tests
void setBackend(std::unique_ptr<ICredentialBackend> backend);
```

### SipProfileManager credential helpers

Convenience methods that derive the credential username from the profile (`authUsername` if set, otherwise `sipUsername`):

```cpp
bool setProfilePassword(const QString &profileId, const QString &password);
bool hasProfilePassword(const QString &profileId) const;
bool removeProfilePassword(const QString &profileId);
```

These delegate entirely to `CredentialStore::instance()`. Profile data is looked up by profileId; the credential username is determined from the profile fields. No username parameter is needed by the caller.

---

## Storage Key Format

Windows Credential Manager target name:

```
SIPClient/sip/<profileId>/<username>
```

- One credential entry per (profileId, username) pair.
- The UserName field is set to `<username>`.
- The CredentialBlob contains the UTF-16 encoded password.
- Persist mode: `CRED_PERSIST_LOCAL_MACHINE`.

---

## Backends

### WindowsCredentialBackend

- Uses `CredWriteW`, `CredReadW`, `CredDeleteW` (Advapi32.lib).
- `isSecure() = true`
- Available only on Windows (`#ifdef Q_OS_WIN`).
- CMake: `target_link_libraries(... Advapi32)`.

### MemoryCredentialBackend

- `QHash<QString, QString>` keyed by `<key>\x1F<username>`.
- `isSecure() = false`.
- **Only for unit tests.** Never used by production code.
- Provides `count()` and `clear()` test helpers.

---

## Logging Rules

| Event | Level | Category | What is logged |
|---|---|---|---|
| Backend selected at startup | INFO | PLATFORM | backend name |
| Credential stored | INFO | PLATFORM | profileId, username, backend name |
| Credential loaded | INFO | PLATFORM | profileId, username, backend name |
| Credential deleted | INFO | PLATFORM | profileId, username, backend name |
| Credential not found | DEBUG | PLATFORM | profileId, username |
| Store / load / delete error | ERROR | PLATFORM | profileId, username, error message |
| Backend unavailable | WARN/ERROR | PLATFORM | reason |

**Password values are never logged at any level.**

---

## Security Rules

- Never pass a password value to `Logger`.
- Never pass a password value to `QSettings`.
- Never include a password value in a `SipProfile` struct field.
- The diagnostic export bundle must not include credentials; `CredentialStore` emits no log payloads containing secrets.
- `MemoryCredentialBackend` is excluded from production use by convention — it has no CMake guard but `isSecure()` returns `false`, which callers can check.

---

## Test Isolation

Tests inject a fresh `MemoryCredentialBackend` via `freshStore()` before each test:

```cpp
static CredentialStore &freshStore()
{
    CredentialStore::instance().setBackend(std::make_unique<MemoryCredentialBackend>());
    return CredentialStore::instance();
}
```

`init()` calls this so each test case starts with an empty in-memory store. The real OS keychain is never touched during tests.

Test file: `tests/test_credential_store.cpp`  
Test QSettings org/app: `IrymeTest` / `SIPClientTest_Credentials`

---

## Test Cases

| # | Name | What it verifies |
|---|---|---|
| 1 | `saveAndLoadPassword` | Store + load round-trip |
| 2 | `deletePasswordRemovesCredential` | Delete removes; subsequent load not-found |
| 3 | `hasPasswordTrueAndFalse` | hasPassword before and after store |
| 4 | `overwritePassword` | Second store overwrites first |
| 5 | `credentialNotFound` | Load on missing credential returns found=false, empty string |
| 6 | `multipleProfiles` | Two profileIds stored and loaded independently |
| 7 | `backendNameReported` | backendName() and isSecureBackendAvailable() reflect the injected backend |
| 8 | `deleteNonExistentIsGraceful` | Delete on missing credential returns false, no crash |
| 9 | `noPasswordInSipProfilePersistence` | QSettings scan: no password/secret/token/credential key or value |
| 10 | `noPasswordInLogOutput` | Logger entries collected during storePassword contain no password string |

---

## Profile Editor Integration (Task 8)

`SipProfileEditorDialog` integrates with `CredentialStore` through `SipProfileManager` helper methods. The dialog itself never touches `CredentialStore` directly.

### Add flow

1. User fills in fields and enters password.
2. Dialog returns `passwordChanged() == true`, `password()` holds the new value.
3. Caller (SidebarPanel): `SipProfileManager::instance().add(p)` → then `setProfilePassword(id, password)`.

### Edit flow

1. Password field is always empty — current password is never fetched or displayed.
2. If user leaves password blank: `passwordChanged() == false` → caller skips credential update → existing credential preserved.
3. If user enters a new password: `passwordChanged() == true` → caller calls `setProfilePassword()` → credential overwritten.

### Delete flow

`SipProfileManager::remove()` now calls `CredentialStore::instance().deletePassword()` before emitting `profileRemoved`. The credential is removed atomically with the profile record. If no credential was stored, the delete is graceful (logged at Debug level, not an error).

---

## Known Limitations

- Linux/macOS backends are not yet implemented; only Windows is supported. On unsupported platforms `CredentialStore` logs a warning and all operations fail.
- `CRED_PERSIST_LOCAL_MACHINE` means credentials are tied to the current machine — no roaming to other devices.
- There is no bulk export or migration path for credentials (by design — credentials are not application data).
