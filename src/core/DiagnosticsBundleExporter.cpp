#include "DiagnosticsBundleExporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>

#include "core/AppSettings.h"
#include "core/CallHistoryStore.h"
#include "core/DiagnosticsTimelineExporter.h"
#include "core/DiagnosticsTimelineService.h"
#include "core/Logger.h"
#include "media/MediaDeviceManager.h"
#include "sip/SipProfileManager.h"
#include "sip/SipTraceLogger.h"

#ifdef HAVE_QT_ZIP_WRITER
#include <private/qzipwriter_p.h>
#endif

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

bool writeTextFile(const QString &path, const QString &text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(text.toUtf8());
    f.close();
    return true;
}

QJsonValue redactedValueJson()
{
    return QJsonValue(DiagnosticsBundleExporter::redactedValue());
}

// AppSettings never stores credentials (SipProfile / QSettings have no
// password field — see CredentialStore/OS keychain), but the key-name
// redaction rule is applied anyway per the bundle export spec, in case a
// future setting does carry something sensitive. Binary blobs (window
// geometry/state) are skipped since they aren't useful diagnostic info.
QJsonObject dumpSettingsRedacted()
{
    QJsonObject out;
    QSettings &settings = AppSettings::settings();
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        if (key.contains(QStringLiteral("geometry"), Qt::CaseInsensitive)
            || key.contains(QStringLiteral("splitter"), Qt::CaseInsensitive)
            || key.startsWith(QStringLiteral("ui/state")))
            continue;

        if (DiagnosticsBundleExporter::isSensitiveKey(key))
            out.insert(key, redactedValueJson());
        else
            out.insert(key, QJsonValue::fromVariant(settings.value(key)));
    }
    return out;
}

QJsonObject sipProfileRedacted(const SipProfile &p)
{
    QJsonObject o;
    o[QStringLiteral("profileId")] = p.profileId;
    o[QStringLiteral("displayName")] = p.displayName;
    o[QStringLiteral("sipUsername")] = p.sipUsername;
    o[QStringLiteral("sipDomain")] = p.sipDomain;
    o[QStringLiteral("sipUri")] = p.sipUri;
    o[QStringLiteral("registrar")] = p.registrar;
    o[QStringLiteral("proxy")] = p.proxy;
    o[QStringLiteral("outboundProxy")] = p.outboundProxy;
    o[QStringLiteral("transport")] = SipProfileManager::transportToString(p.transport);
    // authUsername is not a secret by itself, but its key name contains "auth"
    // and the spec asks for auth-related fields to be redacted uniformly.
    o[QStringLiteral("authUsername")] = redactedValueJson();
    o[QStringLiteral("emergencyServiceUri")] = p.emergencyServiceUri;
    o[QStringLiteral("enableRtt")] = p.enableRtt;
    o[QStringLiteral("enableLmpe")] = p.enableLmpe;
    o[QStringLiteral("enableEtsiCompatibility")] = p.enableEtsiCompatibility;
    // Passwords/auth headers are never stored on SipProfile (they live in
    // CredentialStore / the OS keychain and in-transit SIP headers, the
    // latter already redacted by SipTraceLogger) — called out explicitly so
    // a bundle reader never has to wonder whether they were merely omitted.
    o[QStringLiteral("password")] = redactedValueJson();
    o[QStringLiteral("authHeaders")] = redactedValueJson();
    return o;
}

QJsonObject dumpMediaDevices(const DiagnosticsSnapshot &snapshot);

bool zipFolder(const QString &folderPath, const QString &destinationZipPath, QString *errorOut)
{
#ifdef HAVE_QT_ZIP_WRITER
    QZipWriter writer(destinationZipPath);
    if (!writer.isWritable()) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open %1 for writing").arg(destinationZipPath);
        return false;
    }

    const QDir dir(folderPath);
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString &name : files) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        writer.addFile(name, f.readAll());
        f.close();
    }
    writer.close();
    return writer.status() == QZipWriter::NoError;
#else
    Q_UNUSED(folderPath);
    Q_UNUSED(destinationZipPath);
    if (errorOut)
        *errorOut = QStringLiteral("Qt was built without QZipWriter (Qt6::CorePrivate); "
                                    "using folder export fallback instead.");
    return false;
#endif
}

} // namespace

namespace DiagnosticsBundleExporter {

bool isSensitiveKey(const QString &key)
{
    static const QStringList kSensitiveSubstrings{
        QStringLiteral("password"), QStringLiteral("secret"), QStringLiteral("token"),
        QStringLiteral("authorization"), QStringLiteral("auth"), QStringLiteral("credential")
    };
    for (const QString &needle : kSensitiveSubstrings) {
        if (key.contains(needle, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QString redactedValue()
{
    return QStringLiteral("[REDACTED]");
}

QString defaultFileName()
{
    return QStringLiteral("diagnostics-%1.zip")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));
}

QString defaultDirectory()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) {
        dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                  .filePath(QStringLiteral("Diagnostics"));
    }
    return dir;
}

Result generateBundle(const DiagnosticsSnapshot &snapshot, const QString &destinationPath)
{
    Result result;

    QTemporaryDir staging;
    if (!staging.isValid()) {
        result.error = QStringLiteral("Could not create a temporary staging folder");
        return result;
    }
    const QDir dir(staging.path());

    // diagnostics.json
    writeTextFile(dir.filePath(QStringLiteral("diagnostics.json")),
                  QString::fromUtf8(QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Indented)));

    // timeline.json / timeline.txt — last kMaxEntries (5000) events; the
    // model itself never grows past that cap, so entries() is already bounded.
    const QList<DiagnosticsTimelineEntry> timelineEntries =
        DiagnosticsTimelineService::instance().model()->entries();
    DiagnosticsTimelineExporter::exportJsonToFile(timelineEntries, dir.filePath(QStringLiteral("timeline.json")));
    DiagnosticsTimelineExporter::exportTxtToFile(timelineEntries, dir.filePath(QStringLiteral("timeline.txt")));

    // call_history.json / call_history.csv
    CallHistoryStore::instance().exportToJson(dir.filePath(QStringLiteral("call_history.json")));
    CallHistoryStore::instance().exportToCsv(dir.filePath(QStringLiteral("call_history.csv")));

    // logs.txt
    {
        const QList<LogEntry> entries = Logger::instance().recentEntries();
        QString text;
        if (entries.isEmpty()) {
            text = QStringLiteral("No log entries available");
        } else {
            QStringList lines;
            for (const LogEntry &e : entries)
                lines << formatLogLine(e);
            text = lines.join(QLatin1Char('\n'));
        }
        writeTextFile(dir.filePath(QStringLiteral("logs.txt")), text);
    }

    // sip_trace.txt / sip_trace.json — only a real exporter, never a fabricated ladder.
    if (SipTraceLogger::instance().messages().isEmpty()) {
        writeTextFile(dir.filePath(QStringLiteral("sip_trace.txt")),
                      QStringLiteral("No SIP trace available"));
    } else {
        writeTextFile(dir.filePath(QStringLiteral("sip_trace.txt")), SipTraceLogger::instance().exportToText());
        writeTextFile(dir.filePath(QStringLiteral("sip_trace.json")), SipTraceLogger::instance().exportToJson());
    }

    // settings_redacted.json
    {
        QJsonObject root = dumpSettingsRedacted();
        QJsonArray profiles;
        for (const SipProfile &p : SipProfileManager::instance().profiles())
            profiles.append(sipProfileRedacted(p));
        root[QStringLiteral("sipProfiles")] = profiles;
        writeTextFile(dir.filePath(QStringLiteral("settings_redacted.json")),
                      QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
    }

    // media_devices.json
    writeTextFile(dir.filePath(QStringLiteral("media_devices.json")),
                  QString::fromUtf8(QJsonDocument(dumpMediaDevices(snapshot)).toJson(QJsonDocument::Indented)));

    // system_info.json — reuses the same fields already collected for the
    // Diagnostics Center's System tab (DiagnosticsCollector), so this never
    // duplicates version-detection logic.
    {
        QJsonObject o;
        o[QStringLiteral("appVersion")] = snapshot.appVersion;
        o[QStringLiteral("gitCommit")] = snapshot.gitCommit;
        o[QStringLiteral("qtVersion")] = snapshot.qtVersion;
        o[QStringLiteral("pjsipVersion")] = snapshot.pjsipVersion;
        o[QStringLiteral("platform")] = snapshot.platform;
        o[QStringLiteral("architecture")] = snapshot.architecture;
        o[QStringLiteral("buildType")] = snapshot.buildType;
        o[QStringLiteral("compiler")] = snapshot.compiler;
        o[QStringLiteral("audioCodec")] = snapshot.audioCodec.toJson();
        o[QStringLiteral("videoCodec")] = snapshot.videoCodec.toJson();
        o[QStringLiteral("capturedAtUtc")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        writeTextFile(dir.filePath(QStringLiteral("system_info.json")),
                      QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented)));
    }

    // version.txt
    writeTextFile(dir.filePath(QStringLiteral("version.txt")),
                  QStringLiteral("App version: %1\nGit commit: %2\nBuild type: %3\nQt version: %4\nGenerated: %5\n")
                      .arg(snapshot.appVersion, snapshot.gitCommit, snapshot.buildType, snapshot.qtVersion,
                           QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));

    // Pack the staged folder into a real ZIP if possible; otherwise fall
    // back to keeping the folder itself as the deliverable.
    QString zipError;
    if (zipFolder(staging.path(), destinationPath, &zipError)) {
        result.success = true;
        result.isZip = true;
        result.path = destinationPath;
        return result;
    }

    QDir().mkpath(destinationPath);
    const QStringList files = dir.entryList(QDir::Files);
    bool copyFailed = false;
    for (const QString &name : files) {
        const QString target = QDir(destinationPath).filePath(name);
        QFile::remove(target);
        if (!QFile::copy(dir.filePath(name), target))
            copyFailed = true;
    }
    if (copyFailed) {
        result.error = QStringLiteral("ZIP export unavailable (%1) and folder fallback copy failed")
                           .arg(zipError);
        return result;
    }

    result.success = true;
    result.isZip = false;
    result.path = destinationPath;
    result.error = zipError; // informational: why we fell back to a folder
    return result;
}

} // namespace DiagnosticsBundleExporter

namespace {

QJsonObject mediaDeviceToJson(const MediaDevice &d)
{
    QJsonObject o;
    o[QStringLiteral("id")] = d.id;
    o[QStringLiteral("name")] = d.displayName;
    o[QStringLiteral("isDefault")] = d.isDefault;
    o[QStringLiteral("isAvailable")] = d.isAvailable;
    return o;
}

QJsonObject dumpMediaDevices(const DiagnosticsSnapshot &snapshot)
{
    QJsonObject o;

    QJsonArray mics, speakers, cameras;
    for (const MediaDevice &d : MediaDeviceManager::instance().listMicrophones())
        mics.append(mediaDeviceToJson(d));
    for (const MediaDevice &d : MediaDeviceManager::instance().listSpeakers())
        speakers.append(mediaDeviceToJson(d));
    for (const MediaDevice &d : MediaDeviceManager::instance().listCameras())
        cameras.append(mediaDeviceToJson(d));

    o[QStringLiteral("microphones")] = mics;
    o[QStringLiteral("speakers")] = speakers;
    o[QStringLiteral("cameras")] = cameras;
    o[QStringLiteral("selectedMicrophone")] = snapshot.microphoneName;
    o[QStringLiteral("selectedSpeaker")] = snapshot.speakerName;
    o[QStringLiteral("selectedCamera")] = snapshot.cameraName;
    o[QStringLiteral("microphoneVolume")] = snapshot.microphoneVolume;
    o[QStringLiteral("speakerVolume")] = snapshot.speakerVolume;
    o[QStringLiteral("cameraEnabled")] = snapshot.cameraEnabled;
    o[QStringLiteral("videoMuted")] = snapshot.videoMuted;
    o[QStringLiteral("audioMuted")] = snapshot.audioMuted;
    return o;
}

} // namespace
