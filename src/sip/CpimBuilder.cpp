#include "CpimBuilder.h"

namespace CpimBuilder {

QString build(const QString &from, const QString &to,
              const QString &innerContentType, const QString &body,
              const QDateTime &when)
{
    QString out;
    out += QStringLiteral("From: ") + from + QStringLiteral("\n");
    out += QStringLiteral("To: ") + to + QStringLiteral("\n");
    out += QStringLiteral("DateTime: ") + when.toString(Qt::ISODateWithMs) + QStringLiteral("\n");
    out += QStringLiteral("Content-Type: ") + innerContentType + QStringLiteral("\n");
    out += QStringLiteral("\n");
    out += body;
    return out;
}

} // namespace CpimBuilder
