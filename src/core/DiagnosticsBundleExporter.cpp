#include "DiagnosticsBundleExporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

#include "core/AppSettings.h"
#include "core/CallHistoryStore.h"
#include "core/Logger.h"
#include "sip/SipTraceLogger.h"

namespace {

QString formatLogLine(const LogEntry &e)
{
    return QStringLiteral("[%1] [%2] [%3] %4%5")
        .arg(e.timestamp.toString(Qt::ISODateWithMs),
             Logger::levelName(e.level),
             Logger::categoryName(e.category),
             e.message,
             e.payload.isEmpty() ? QString() : QStringLiteral(" | %1").arg(e.payload));
}

// AppSettings never stores credentials (SipProfile / QSettings have no
// password field — see CredentialStore/OS keychain), so no key needs to be
// excluded on those grounds. Binary blobs (window geometry/state) are still
// skipped here since they aren't useful diagnostic information.
QJsonObject dumpSettings()
{
    QJsonObject out;
    QSettings &settings = AppSettings::settings();
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        if (key.contains(QStringLiteral("geometry"), Qt::CaseInsensitive)
            || key.contains(QStringLiteral("splitter"), Qt::CaseInsensitive)
            || key.startsWith(QStringLiteral("ui/state")))
            continue;

        const QVariant value = settings.value(key);
        out.insert(key, QJsonValue::fromVariant(value));
    }
    return out;
}

} // namespace

namespace DiagnosticsBundleExporter {

QString generateBundle(const DiagnosticsSnapshot &snapshot, QString *errorOut)
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString bundleDir = QDir(baseDir).filePath(
        QStringLiteral("SIPClient-diagnostics-%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"))));

    QDir dir;
    if (!dir.mkpath(bundleDir)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not create bundle folder: %1").arg(bundleDir);
        return {};
    }

    // logs.txt
    {
        QFile f(QDir(bundleDir).filePath(QStringLiteral("logs.txt")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QStringList lines;
            for (const LogEntry &e : Logger::instance().recentEntries())
                lines << formatLogLine(e);
            f.write(lines.join(QLatin1Char('\n')).toUtf8());
            f.close();
        }
    }

    // sip_ladder.json
    if (!SipTraceLogger::instance().messages().isEmpty()) {
        QFile f(QDir(bundleDir).filePath(QStringLiteral("sip_ladder.json")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(SipTraceLogger::instance().exportToJson().toUtf8());
            f.close();
        }
    }

    // call_history.json
    CallHistoryStore::instance().exportToJson(QDir(bundleDir).filePath(QStringLiteral("call_history.json")));

    // settings.json
    {
        QFile f(QDir(bundleDir).filePath(QStringLiteral("settings.json")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(dumpSettings()).toJson(QJsonDocument::Indented));
            f.close();
        }
    }

    // diagnostics.json
    {
        QFile f(QDir(bundleDir).filePath(QStringLiteral("diagnostics.json")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Indented));
            f.close();
        }
    }

    return bundleDir;
}

} // namespace DiagnosticsBundleExporter
