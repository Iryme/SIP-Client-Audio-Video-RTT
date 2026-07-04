#pragma once
#include <QString>

#include "core/DiagnosticsSnapshot.h"

// Foundation for the Diagnostics Center's "Generate Diagnostics Bundle" button.
// Writes a set of plain files into a fresh temporary folder:
//   logs.txt            - Logger::recentEntries(), if any
//   sip_ladder.json      - SipTraceLogger::exportToJson(), if any messages exist
//   call_history.json    - CallHistoryStore::exportToJson()
//   settings.json         - AppSettings key/value dump (no passwords: SipProfile /
//                            AppSettings never store credentials — those live in
//                            CredentialStore/OS keychain and are never read here)
//   diagnostics.json      - the given DiagnosticsSnapshot, serialized
//
// Zipping the folder is a follow-up task; this returns the folder path so the
// caller can show it to the user or open it in Explorer.
namespace DiagnosticsBundleExporter {

// Returns the created bundle folder path, or an empty string on failure
// (errorOut, if non-null, is set to a human-readable reason).
QString generateBundle(const DiagnosticsSnapshot &snapshot, QString *errorOut = nullptr);

} // namespace DiagnosticsBundleExporter
