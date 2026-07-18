# macOS Platform Services

W113G isolates platform behavior behind existing boundaries:

- SIP credentials use `MacKeychainBackend` and the macOS Security framework.
  The Keychain service is the credential key and the account is the SIP user;
  passwords are never logged or written to QSettings.
- Application data, settings, history, diagnostics, and user-selected exports
  continue to use Qt's `QStandardPaths`, `QSettings`, and file dialogs.
- URLs continue to open through `QDesktopServices`.
- Windows Credential Manager and GDI renderer sources remain Windows-only.

Camera and microphone access uses Qt Multimedia/PJSIP plus macOS privacy
consent. The bundle declares both usage strings; users can change permission
later under System Settings > Privacy & Security > Microphone or Camera.

The automated credential tests inject the memory backend and therefore do not
create persistent Keychain items. A real Keychain save/load/delete should be
confirmed manually with a disposable SIP profile on a test Mac.

