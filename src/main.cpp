#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QProcessEnvironment>
#include <QTextStream>
#include <cstring>
#include "AppVersion.h"
#include "app/Application.h"
#include "core/AppSettings.h"
#include "core/PerfScope.h"
#include "sip/RtpPortRangeConfig.h"
#include "sip/SipProfileManager.h"

namespace {

// Task W109A — resolves --config-dir/--rtp-port-start/--rtp-port-end (and
// their SIPCLIENT_* environment variable equivalents) before any QSettings
// object is constructed, so two instances of this app on the same host can
// use separate profile/settings stores and non-overlapping RTP ranges. Must
// run before Application/QApplication is constructed (Application's ctor
// already touches AppSettings via ThemeManager).
void applyEarlyCommandLineOverrides(int argc, char **argv)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("SIP Client - Audio / Video / RTT"));
    parser.addHelpOption();

    QCommandLineOption configDirOpt(
        QStringLiteral("config-dir"),
        QStringLiteral("Directory for this instance's settings/profile store "
                       "(lets two instances run on the same host without "
                       "sharing settings). Overrides SIPCLIENT_CONFIG_DIR."),
        QStringLiteral("path"));
    QCommandLineOption rtpStartOpt(
        QStringLiteral("rtp-port-start"),
        QStringLiteral("Start of the local RTP/RTCP media port range for this "
                       "instance (must be even). Overrides SIPCLIENT_RTP_PORT_START "
                       "and any persisted media/rtpPortStart setting for this run only."),
        QStringLiteral("port"));
    QCommandLineOption rtpEndOpt(
        QStringLiteral("rtp-port-end"),
        QStringLiteral("End of the local RTP/RTCP media port range for this "
                       "instance. Overrides SIPCLIENT_RTP_PORT_END and any "
                       "persisted media/rtpPortEnd setting for this run only."),
        QStringLiteral("port"));
    parser.addOption(configDirOpt);
    parser.addOption(rtpStartOpt);
    parser.addOption(rtpEndOpt);

    // process() calls QCoreApplication::instance()->arguments() if available;
    // no QApplication exists yet, so parse the raw argv directly instead.
    QStringList args;
    for (int i = 0; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    parser.parse(args);

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    QString configDir = parser.value(configDirOpt);
    if (configDir.isEmpty())
        configDir = env.value(QStringLiteral("SIPCLIENT_CONFIG_DIR"));
    if (!configDir.isEmpty()) {
        QDir().mkpath(configDir);
        AppSettings::setConfigDirectoryOverride(configDir);
        SipProfileManager::setConfigDirectoryOverride(configDir);
    }

    bool ok1 = false, ok2 = false;
    QString startStr = parser.value(rtpStartOpt);
    if (startStr.isEmpty())
        startStr = env.value(QStringLiteral("SIPCLIENT_RTP_PORT_START"));
    QString endStr = parser.value(rtpEndOpt);
    if (endStr.isEmpty())
        endStr = env.value(QStringLiteral("SIPCLIENT_RTP_PORT_END"));

    const int start = startStr.toInt(&ok1);
    const int end   = endStr.toInt(&ok2);
    if (ok1 && ok2)
        setRtpPortRangeSessionOverride(start, end);
}

} // namespace

int main(int argc, char *argv[])
{
    // A dependency-loading smoke test for packaged bundles.  This path runs
    // before QApplication, opens no windows, touches no profiles or devices,
    // and still proves that the Mach-O executable and its linked libraries can
    // be loaded from the relocated bundle.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--bundle-smoke-test") == 0) {
            QTextStream(stdout) << "SIP Client " << APP_VERSION_STRING
                                << " bundle smoke test PASS\n";
            return 0;
        }
    }

    PerfScope::initAppClock();

    applyEarlyCommandLineOverrides(argc, argv);

    Application app(argc, argv);

    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] main(): startup complete, entering event loop at T+%1 ms")
            .arg(PerfScope::msecsSinceAppStart()));

    return app.exec();
}
