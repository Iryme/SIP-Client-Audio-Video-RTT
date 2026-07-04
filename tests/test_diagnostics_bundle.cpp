#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/DiagnosticsBundleExporter.h"
#include "sip/SipTraceLogger.h"

#ifdef HAVE_QT_ZIP_WRITER
#include <private/qzipreader_p.h>
#endif

namespace {

// Reads back the bundle's file manifest + contents regardless of whether it
// landed as a real ZIP (HAVE_QT_ZIP_WRITER) or a fallback folder, so the same
// test logic works either way.
QHash<QString, QByteArray> readBundleFiles(const QString &path)
{
    QHash<QString, QByteArray> out;
    const QFileInfo info(path);

    if (info.isDir()) {
        const QDir dir(path);
        for (const QString &name : dir.entryList(QDir::Files))
            out.insert(name, [&] {
                QFile f(dir.filePath(name));
                f.open(QIODevice::ReadOnly);
                return f.readAll();
            }());
        return out;
    }

#ifdef HAVE_QT_ZIP_WRITER
    QZipReader reader(path);
    for (const auto &fileInfo : reader.fileInfoList())
        out.insert(fileInfo.filePath, reader.fileData(fileInfo.filePath));
#endif
    return out;
}

} // namespace

class TestDiagnosticsBundle : public QObject
{
    Q_OBJECT

private slots:
    // Must run before anything else generates SIP trace messages, so
    // sip_trace.txt still exercises the "nothing recorded yet" fallback text.
    // (logs.txt can't be tested the same way: DiagnosticsTimelineService's
    // first touch — required to build timeline.json/.txt — always logs at
    // least one real "RTT session controller created" line as a side effect
    // of constructing SipManager/RttSession, so an empty Logger is not a
    // reachable state once a bundle has ever been generated in-process.)
    void fallbackWhenSipTraceMissing();

    void isSensitiveKeyMatchesExpectedSubstrings();
    void bundleManifestContainsExpectedFiles();
    void timelineIncludedInBundle();
    void callHistoryIncludedInBundle();
    void settingsRedaction();
};

void TestDiagnosticsBundle::fallbackWhenSipTraceMissing()
{
    QVERIFY2(SipTraceLogger::instance().messages().isEmpty(),
             "test must run before any SIP trace messages are recorded");

    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());
    const QString dest = QDir(outDir.path()).filePath(QStringLiteral("bundle.zip"));

    const DiagnosticsSnapshot snapshot;
    const DiagnosticsBundleExporter::Result result = DiagnosticsBundleExporter::generateBundle(snapshot, dest);
    QVERIFY(result.success);

    const QHash<QString, QByteArray> files = readBundleFiles(result.path);
    QVERIFY(files.contains(QStringLiteral("sip_trace.txt")));
    QCOMPARE(QString::fromUtf8(files.value(QStringLiteral("sip_trace.txt"))), QStringLiteral("No SIP trace available"));
    QVERIFY2(!files.contains(QStringLiteral("sip_trace.json")),
             "sip_trace.json should be omitted rather than containing a fabricated empty ladder");
}

void TestDiagnosticsBundle::isSensitiveKeyMatchesExpectedSubstrings()
{
    QVERIFY(DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("account/password")));
    QVERIFY(DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("api.Secret")));
    QVERIFY(DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("oauth_token")));
    QVERIFY(DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("Authorization")));
    QVERIFY(DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("authUsername")));
    QVERIFY(DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("credential_store/key")));

    QVERIFY(!DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("ui/theme")));
    QVERIFY(!DiagnosticsBundleExporter::isSensitiveKey(QStringLiteral("media/device/microphone")));
}

void TestDiagnosticsBundle::bundleManifestContainsExpectedFiles()
{
    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());
    const QString dest = QDir(outDir.path()).filePath(QStringLiteral("bundle.zip"));

    const DiagnosticsSnapshot snapshot;
    const DiagnosticsBundleExporter::Result result = DiagnosticsBundleExporter::generateBundle(snapshot, dest);
    QVERIFY(result.success);

    const QHash<QString, QByteArray> files = readBundleFiles(result.path);
    static const QStringList kAlwaysPresent{
        QStringLiteral("diagnostics.json"), QStringLiteral("timeline.json"), QStringLiteral("timeline.txt"),
        QStringLiteral("call_history.json"), QStringLiteral("call_history.csv"), QStringLiteral("logs.txt"),
        QStringLiteral("sip_trace.txt"), QStringLiteral("settings_redacted.json"),
        QStringLiteral("media_devices.json"), QStringLiteral("system_info.json"), QStringLiteral("version.txt")
    };
    for (const QString &name : kAlwaysPresent)
        QVERIFY2(files.contains(name), qPrintable(QStringLiteral("missing %1").arg(name)));
}

void TestDiagnosticsBundle::timelineIncludedInBundle()
{
    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());
    const QString dest = QDir(outDir.path()).filePath(QStringLiteral("bundle.zip"));

    const DiagnosticsSnapshot snapshot;
    const DiagnosticsBundleExporter::Result result = DiagnosticsBundleExporter::generateBundle(snapshot, dest);
    QVERIFY(result.success);

    const QHash<QString, QByteArray> files = readBundleFiles(result.path);
    QVERIFY(files.contains(QStringLiteral("timeline.json")));
    const QJsonDocument doc = QJsonDocument::fromJson(files.value(QStringLiteral("timeline.json")));
    QVERIFY(doc.isObject());
    QVERIFY(doc.object().contains(QStringLiteral("entries")));
    QVERIFY(doc.object().value(QStringLiteral("entries")).isArray());
}

void TestDiagnosticsBundle::callHistoryIncludedInBundle()
{
    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());
    const QString dest = QDir(outDir.path()).filePath(QStringLiteral("bundle.zip"));

    const DiagnosticsSnapshot snapshot;
    const DiagnosticsBundleExporter::Result result = DiagnosticsBundleExporter::generateBundle(snapshot, dest);
    QVERIFY(result.success);

    const QHash<QString, QByteArray> files = readBundleFiles(result.path);
    QVERIFY(files.contains(QStringLiteral("call_history.json")));
    const QJsonDocument doc = QJsonDocument::fromJson(files.value(QStringLiteral("call_history.json")));
    QVERIFY(doc.isArray());

    QVERIFY(files.contains(QStringLiteral("call_history.csv")));
    const QString csv = QString::fromUtf8(files.value(QStringLiteral("call_history.csv")));
    QVERIFY(csv.startsWith(QStringLiteral("id,direction,remoteUri")));
}

void TestDiagnosticsBundle::settingsRedaction()
{
    const QString key = QStringLiteral("test/passwordField");
    AppSettings::settings().setValue(key, QStringLiteral("super-secret-value"));

    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());
    const QString dest = QDir(outDir.path()).filePath(QStringLiteral("bundle.zip"));

    const DiagnosticsSnapshot snapshot;
    const DiagnosticsBundleExporter::Result result = DiagnosticsBundleExporter::generateBundle(snapshot, dest);

    AppSettings::settings().remove(key);
    QVERIFY(result.success);

    const QHash<QString, QByteArray> files = readBundleFiles(result.path);
    QVERIFY(files.contains(QStringLiteral("settings_redacted.json")));
    const QString text = QString::fromUtf8(files.value(QStringLiteral("settings_redacted.json")));
    QVERIFY(!text.contains(QStringLiteral("super-secret-value")));

    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value(key).toString(), DiagnosticsBundleExporter::redactedValue());
    QVERIFY(doc.object().contains(QStringLiteral("sipProfiles")));
}

QTEST_GUILESS_MAIN(TestDiagnosticsBundle)
#include "test_diagnostics_bundle.moc"
