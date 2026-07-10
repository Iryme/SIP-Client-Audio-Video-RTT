#include "SipRawMessageParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

QString normalizeLineEndings(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

// Returns the header value for the first line matching "name:" (case
// insensitive), trimmed. Recognizes the compact forms passed in altName
// (e.g. "i" for Call-ID, "l" for Content-Length).
QString findHeaderValue(const QStringList &headerLines, const QString &name,
                        const QString &altName = QString())
{
    for (const QString &line : headerLines) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString key = line.left(colon).trimmed();
        if (key.compare(name, Qt::CaseInsensitive) == 0
            || (!altName.isEmpty() && key.compare(altName, Qt::CaseInsensitive) == 0)) {
            return line.mid(colon + 1).trimmed();
        }
    }
    return QString();
}

} // namespace

SipMessageTrace SipRawMessageParser::parse(const QString &rawText,
                                            SipMessageTrace::Direction direction)
{
    SipMessageTrace trace;
    trace.direction = direction;
    trace.rawSip    = rawText;

    const QString normalized = normalizeLineEndings(rawText);
    const int sep = normalized.indexOf(QStringLiteral("\n\n"));
    const QString headerBlock = sep >= 0 ? normalized.left(sep) : normalized;

    QStringList lines = headerBlock.split(QLatin1Char('\n'));
    if (lines.isEmpty())
        return trace;

    const QString startLine = lines.takeFirst().trimmed();

    // Status line: "SIP/2.0 200 OK". Request line: "INVITE sip:... SIP/2.0".
    static const QRegularExpression statusLineRe(
        QStringLiteral("^SIP/2\\.0\\s+(\\d{3})\\s*(.*)$"));
    const QRegularExpressionMatch statusMatch = statusLineRe.match(startLine);
    if (statusMatch.hasMatch()) {
        trace.statusCode = statusMatch.captured(1).toInt();
        trace.statusText = statusMatch.captured(2).trimmed();
    } else {
        const int firstSpace = startLine.indexOf(QLatin1Char(' '));
        trace.method = (firstSpace > 0 ? startLine.left(firstSpace) : startLine).toUpper();
    }

    trace.callId      = findHeaderValue(lines, QStringLiteral("Call-ID"), QStringLiteral("i"));
    trace.cSeq        = findHeaderValue(lines, QStringLiteral("CSeq"));
    trace.fromUri     = findHeaderValue(lines, QStringLiteral("From"), QStringLiteral("f"));
    trace.toUri       = findHeaderValue(lines, QStringLiteral("To"), QStringLiteral("t"));
    trace.contentType = findHeaderValue(lines, QStringLiteral("Content-Type"), QStringLiteral("c"));
    trace.contentEncoding = findHeaderValue(lines, QStringLiteral("Content-Encoding"), QStringLiteral("e"));

    // CSeq method suffix ("312 INVITE") gives us the method for responses too.
    if (trace.method.isEmpty() && !trace.cSeq.isEmpty()) {
        const QStringList cseqParts = trace.cSeq.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (cseqParts.size() >= 2)
            trace.method = cseqParts.last().toUpper();
    }

    return trace;
}
