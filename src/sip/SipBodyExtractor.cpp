#include "SipBodyExtractor.h"

#include <QList>

namespace {

// Splits a byte header block into lines on bare '\n', stripping a trailing
// '\r' from each line if present. Byte-level only — never touches the body.
QList<QByteArray> splitHeaderLines(const QByteArray &headerBlock)
{
    QList<QByteArray> lines;
    int start = 0;
    while (start <= headerBlock.size()) {
        int nl = headerBlock.indexOf('\n', start);
        QByteArray line = (nl < 0) ? headerBlock.mid(start) : headerBlock.mid(start, nl - start);
        if (line.endsWith('\r'))
            line.chop(1);
        lines.append(line);
        if (nl < 0)
            break;
        start = nl + 1;
    }
    return lines;
}

QByteArray findHeaderValueBytes(const QList<QByteArray> &lines, const char *name, const char *altName)
{
    for (const QByteArray &line : lines) {
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;
        QByteArray key = line.left(colon).trimmed();
        if (key.compare(name, Qt::CaseInsensitive) == 0
            || (altName && key.compare(altName, Qt::CaseInsensitive) == 0)) {
            return line.mid(colon + 1).trimmed();
        }
    }
    return QByteArray();
}

} // namespace

namespace SipBodyExtractor {

Result extract(const QString &rawSip)
{
    Result result;

    // rawSip is a Latin-1 round-trip of the original wire bytes (see
    // PjsipTraceModule.cpp) — toLatin1() recovers those bytes exactly,
    // including any embedded NUL bytes or non-UTF-8 binary content.
    const QByteArray raw = rawSip.toLatin1();

    int sep = raw.indexOf("\r\n\r\n");
    int sepLen = 4;
    const int altSep = raw.indexOf("\n\n");
    if (sep < 0 || (altSep >= 0 && altSep < sep)) {
        // No CRLFCRLF found, or a bare LFLF appears earlier (some captures
        // normalize line endings before this class ever sees them) — prefer
        // whichever separator occurs first.
        if (altSep >= 0 && (sep < 0 || altSep < sep)) {
            sep = altSep;
            sepLen = 2;
        }
    }

    if (sep < 0)
        return result; // no header/body boundary — nothing to extract

    result.headerFound = true;
    result.headerBlock = raw.left(sep);

    const QByteArray afterSeparator = raw.mid(sep + sepLen);

    const QList<QByteArray> headerLines = splitHeaderLines(result.headerBlock);
    const QByteArray contentLengthValue =
        findHeaderValueBytes(headerLines, "Content-Length", "l");

    bool ok = false;
    const qint64 contentLength = contentLengthValue.trimmed().toLongLong(&ok);
    if (ok && contentLength >= 0 && contentLength <= afterSeparator.size()) {
        result.rawBodyBytes = afterSeparator.left(static_cast<int>(contentLength));
        result.contentLengthUsed = true;
    } else {
        // No usable Content-Length — fall back to "everything after the
        // separator" (matches the pre-existing text-based behavior), still
        // without any CRLF normalization or trimming of the body bytes.
        result.rawBodyBytes = afterSeparator;
    }

    return result;
}

} // namespace SipBodyExtractor
