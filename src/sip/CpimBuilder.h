#pragma once
#include <QDateTime>
#include <QString>

// Minimal RFC 3862 CPIM wrapper generator — the serialization counterpart of
// CpimParser (Task W090, parse-only). Produces the same header-block +
// blank-line + body shape CpimParser::parse() expects, so a message this
// class builds round-trips through the existing parser unchanged.
//
// Pure Qt, no PJSIP dependency, no network I/O — this only builds text.
namespace CpimBuilder {

// from/to: CPIM "From"/"To" header values (e.g. "sip:alice@example.com").
// innerContentType: the wrapped body's real Content-Type (e.g. "text/plain; charset=utf-8").
// body: the user-authored message text (UTF-8 safe; not otherwise transformed).
// when: CPIM DateTime header value; defaults to current UTC time.
QString build(const QString &from, const QString &to,
              const QString &innerContentType, const QString &body,
              const QDateTime &when = QDateTime::currentDateTimeUtc());

} // namespace CpimBuilder
