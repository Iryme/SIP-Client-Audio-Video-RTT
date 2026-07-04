#pragma once
#include <QString>

#include "core/DiagnosticsSnapshot.h"

// Diagnostics Center's "Generate Diagnostics Bundle" button: stages a full
// set of diagnostic files (diagnostics.json, timeline.json/.txt,
// call_history.json/.csv, logs.txt, sip_trace.json/.txt, settings_redacted.json,
// media_devices.json, system_info.json, version.txt) into a temp folder, then
// packs that folder into a single ZIP file the user can hand to support.
//
// Real ZIP output requires Qt's private QZipWriter (Qt6::CorePrivate) — see
// HAVE_QT_ZIP_WRITER, defined by CMake when that target is available. If it
// is not available in a given Qt build, generateBundle() falls back to
// leaving the staged folder in place (destinationPath is then a directory,
// not a .zip file) rather than inventing a bespoke ZIP writer.
namespace DiagnosticsBundleExporter {

// Suggested file name, e.g. "diagnostics-20260704-153000.zip".
QString defaultFileName();

// Suggested starting directory for a QFileDialog (Documents, falling back to
// AppData/Diagnostics if Documents is unavailable).
QString defaultDirectory();

struct Result
{
    bool    success{false};
    QString path;      // the .zip file (or, on fallback, the folder) written
    bool    isZip{false};
    QString error;
};

// Redaction policy, exposed for tests: a settings/profile field key is
// considered sensitive (and its value replaced with redactedValue()) if it
// contains any of "password", "secret", "token", "authorization", "auth",
// "credential" (case-insensitive).
bool isSensitiveKey(const QString &key);
QString redactedValue();

// Builds the bundle and writes it to destinationPath (a full file path,
// typically ending in ".zip", chosen by the caller e.g. via QFileDialog).
Result generateBundle(const DiagnosticsSnapshot &snapshot, const QString &destinationPath);

} // namespace DiagnosticsBundleExporter
