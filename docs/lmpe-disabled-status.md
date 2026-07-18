# LMPE Disabled Status (Task W113F)

LMPE has no interoperable wire format yet (`src/etsi/UnconfirmedLmpeCodec.h`
always returns a blocked result — see `docs/agent-results/W103-lmpe-foundation-result.md`).
This was always true at the protocol/codec level, but before Task W113F the
UI surface didn't reflect that: a user could check "Enable LMPE" in the
profile editor, open a fully interactive "LMPE" tab with its own input box
and Send button, and see local-echoed text appear in a list — none of which
ever sent anything anywhere, but nothing told the user that.

## What's disabled and where

| Surface | Before | After |
|---|---|---|
| `SipProfileEditorDialog`'s "Enable LMPE" checkbox | Enabled, checkable, saved to profile | Disabled (`setEnabled(false)`), label "Enable LMPE — unavailable", tooltip explaining why, force-unchecked always |
| `RttPanel`'s "LMPE" tab | Full `QListWidget` + `QLineEdit` + Send button, local echo only | Single disabled label "LMPE — unavailable" with the same tooltip, no input/send/list widgets at all |
| `CallWorkspacePanel`'s LMPE status card | `"—"` (ambiguous — looked like "not checked yet") | `"Unavailable"`, always |
| `SipProfile::enableLmpe` persistence | Loaded/saved as whatever was stored | `SipProfileManager::loadAllProfiles()` forces it `false` at the single authoritative load point, regardless of the saved value, with a redacted warning log (profile id only) if an old value was `true` |
| `SipProfileEditorDialog::toProfile()` (save path) | Saved the checkbox's checked state | Always saves `false`, independent of the (now permanently unchecked) checkbox |

## Runtime guarantee

`CallMediaOptions::enableLmpe` (the field that would actually gate any
LMPE behavior in a call) is **never set from `SipProfile::enableLmpe`
anywhere in the codebase** — confirmed by a full-repo grep before this
task started. LMPE was already fully inert at the call/media level; W113F
closes the *appearance* gap (checkbox, tab, status card) and the
*persistence* gap (an old/imported config with `enableLmpe: true` no
longer resurrects a checked-looking control), but did not need to touch
any call-setup code, since there was never a real "enable" path to disable
there.

## Migration / stale config

An old saved profile (or a hand-edited/imported JSON export) with
`enableLmpe: true` loads without crashing: `SipProfileManager` logs one
redacted warning (`"Profile <id> has enableLmpe=true in saved settings —
LMPE is unavailable, forcing disabled at load"` — never any message
content or account credentials) and proceeds with `enableLmpe = false` for
that profile in memory. The next time that profile is saved through the
editor dialog, the stored value is corrected to `false` too.

## Tests

`tests/test_sip_profile_manager.cpp`:
`enableLmpeForcedFalseOnLoadRegardlessOfSavedValue` — adds a profile with
`enableLmpe=true` through one `SipProfileManager` instance, then
constructs a **fresh** instance (forcing a real reload from `QSettings`,
the same path a restarted application takes) and asserts the reloaded
profile's `enableLmpe` is `false`.

No automated test exists for the `RttPanel`/`SipProfileEditorDialog`/
`CallWorkspacePanel` UI changes themselves — none of those classes have an
existing widget-level test harness (consistent with the rest of this
session's UI work), so these are verified by code reading plus the full
Debug/Release rebuild, not a dedicated GUI test.
