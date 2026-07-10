#pragma once
#include <QString>

// Redacts a transfer URL (e.g. an RCS FT HTTP file-transfer download URL) so
// it is safe to show in a diagnostics preview or export by default. Such
// URLs commonly carry a one-time download token or session identifier in
// the query string or path — redaction keeps only the scheme/host/port and
// a truncated path, so an operator can recognize *where* the file would
// come from without leaking a live-usable credential into logs/exports.
namespace UrlRedactor {

// scheme://host[:port]/partial-path[+hash of the remainder]#... ; query
// parameters are always dropped. Returns an empty string for an empty/
// unparseable input.
QString redact(const QString &url);

} // namespace UrlRedactor
