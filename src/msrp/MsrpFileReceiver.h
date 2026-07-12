#pragma once
#include <QByteArray>
#include <QString>

// MSRP file transfer receive-side helper (Task W104, RFC 5547). Isolated
// from MsrpSession/MsrpChunkAssembler on purpose: the assembled message
// body already sits fully in memory (bounded by MsrpSession's existing
// maxMessageBytes cap, same as any other MSRP message — see
// docs/msrp-foundation.md section 7), so writing it to disk is a one-shot,
// caller-triggered action, never something the wire layer does on its own.
namespace MsrpFileReceiver {

struct SaveResult
{
    bool ok{false};
    QString error;
    qint64 bytesWritten{0};
};

// Writes body to savePath atomically (via QSaveFile: either the full
// content lands at savePath, or nothing does — never a truncated partial
// file on failure). savePath is always caller-supplied (a save dialog
// result or test fixture path) — this function never derives a path from
// peer-supplied data itself.
SaveResult saveToPath(const QByteArray &body, const QString &savePath);

// Verifies body against an expected hash from a negotiated file-selector.
// algorithm is case-insensitive; only "sha-1"/"sha1" and "sha-256"/"sha256"
// are recognized. Returns true (verification "passes") when expectedHex is
// empty, since an unspecified hash means nothing to check — callers that
// require a hash must check expectedHex.isEmpty() themselves first.
bool verifyHash(const QByteArray &body, const QString &algorithm, const QString &expectedHex);

} // namespace MsrpFileReceiver
