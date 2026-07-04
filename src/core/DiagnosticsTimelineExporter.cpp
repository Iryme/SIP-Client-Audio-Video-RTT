#include "DiagnosticsTimelineExporter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace DiagnosticsTimelineExporter {

QString toJsonString(const QList<DiagnosticsTimelineEntry> &entries)
{
    QJsonArray arr;
    for (const auto &e : entries)
        arr.append(e.toJson());

    QJsonObject root;
    root.insert(QStringLiteral("entries"), arr);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString toTxtString(const QList<DiagnosticsTimelineEntry> &entries)
{
    QString out;
    QTextStream ts(&out);
    for (const auto &e : entries) {
        ts << QStringLiteral("[%1] [%2] [%3] %4")
                  .arg(e.timestamp.toString(Qt::ISODateWithMs),
                       timelineCategoryName(e.category),
                       timelineSeverityName(e.severity),
                       e.title);
        if (!e.details.isEmpty())
            ts << QStringLiteral(" | %1").arg(e.details);
        if (e.sipCode > 0)
            ts << QStringLiteral(" | sip=%1").arg(e.sipCode);
        if (!e.remoteUri.isEmpty())
            ts << QStringLiteral(" | uri=%1").arg(e.remoteUri);
        ts << '\n';
    }
    return out;
}

bool exportJsonToFile(const QList<DiagnosticsTimelineEntry> &entries, const QString &path, QString *errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open %1 for writing").arg(path);
        return false;
    }
    f.write(toJsonString(entries).toUtf8());
    return true;
}

bool exportTxtToFile(const QList<DiagnosticsTimelineEntry> &entries, const QString &path, QString *errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open %1 for writing").arg(path);
        return false;
    }
    f.write(toTxtString(entries).toUtf8());
    return true;
}

} // namespace DiagnosticsTimelineExporter
