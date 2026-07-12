#pragma once
#include <QString>
#include <QStringList>

// RFC 5547 "file-selector" SDP attribute (a=file-selector) — parses/builds
// the structured value only (name/size/type/hash). Pure text, no network
// access, no filesystem access (Task W104, mirrors MsrpSdpNegotiator's
// existing pure-parsing style).
//
// fileName here is always a peer-supplied (or locally chosen) DISPLAY
// label, never a filesystem path — see sanitizeFileNameForDisplay(), which
// is the only sanctioned way to turn an untrusted name into something safe
// to show in a save dialog. Nothing in this file ever touches disk.
struct MsrpFileSelectorInfo
{
    bool ok{false};
    QString fileName;
    qint64 fileSize{-1};    // -1 = not present in the attribute
    QString fileType;       // MIME type, verbatim
    QString hashAlgorithm;  // e.g. "sha-1", lowercased
    QString hashValueHex;   // hex digits only (no ':' separators), uppercased
    QStringList warnings;
};

namespace MsrpFileSelector {

// Parses one a=file-selector attribute value (without the leading
// "a=file-selector:"). Tolerant of unknown/malformed tokens — never throws,
// never fatal; check .ok / .warnings.
MsrpFileSelectorInfo parse(const QString &attributeValue);

// Builds the RFC 5547 file-selector attribute value (no leading
// "a=file-selector:"). Fields with an empty/negative value are omitted.
// hashValueHex is expected as plain hex (no separators); colons are
// inserted between octet pairs to match RFC 5547's examples.
QString build(const QString &fileName, qint64 fileSize, const QString &fileType,
              const QString &hashAlgorithm, const QString &hashValueHex);

// Reduces a peer-supplied (or otherwise untrusted) file name to a safe
// basename suitable only as a suggested default in a save dialog: strips
// any directory components (both '/' and '\\'), rejects "." / ".." and an
// empty result. Never returns a path, never used to open a file directly —
// callers must still let the user (or a caller-controlled directory)
// choose the actual save location.
QString sanitizeFileNameForDisplay(const QString &rawName);

} // namespace MsrpFileSelector
