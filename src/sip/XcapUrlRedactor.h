#pragma once
#include <QString>

// XCAP URL redaction (Task W099, requirement 7): strips userinfo, any
// token-bearing query string, and folds everything past the first few path
// segments into a fingerprint — while still keeping scheme/host/port and
// enough of the path (AUID + users-or-global + a partial selector) to be
// useful in diagnostics. Independent of UrlRedactor (Task W095), which is
// tuned for RCS download URLs, not XCAP document selectors.
namespace XcapUrlRedactor {

QString redact(const QString &url);

} // namespace XcapUrlRedactor
