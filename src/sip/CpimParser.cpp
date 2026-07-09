#include "CpimParser.h"

#include <QStringList>

namespace {

QString normalizeLineEndings(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString findHeaderValue(const QStringList &headerLines, const QString &name)
{
    for (const QString &line : headerLines) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString key = line.left(colon).trimmed();
        if (key.compare(name, Qt::CaseInsensitive) == 0)
            return line.mid(colon + 1).trimmed();
    }
    return QString();
}

} // namespace

CpimInfo CpimParser::parse(const QString &body)
{
    CpimInfo info;
    if (body.trimmed().isEmpty())
        return info;

    const QString normalized = normalizeLineEndings(body);
    const int sep = normalized.indexOf(QStringLiteral("\n\n"));
    const QString headerBlock = sep >= 0 ? normalized.left(sep) : normalized;

    const QStringList lines = headerBlock.split(QLatin1Char('\n'));

    info.from        = findHeaderValue(lines, QStringLiteral("From"));
    info.to          = findHeaderValue(lines, QStringLiteral("To"));
    info.dateTime    = findHeaderValue(lines, QStringLiteral("DateTime"));
    info.subject     = findHeaderValue(lines, QStringLiteral("Subject"));
    info.contentType = findHeaderValue(lines, QStringLiteral("Content-Type"));
    info.wrappedBody = sep >= 0 ? normalized.mid(sep + 2).trimmed() : QString();

    // Consider the message a CPIM wrapper if at least one recognized CPIM
    // header was present — an empty/garbled body should not be reported as
    // valid CPIM.
    info.present = !info.from.isEmpty() || !info.to.isEmpty()
                   || !info.dateTime.isEmpty() || !info.subject.isEmpty()
                   || !info.contentType.isEmpty();

    return info;
}
