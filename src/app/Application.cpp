#include "Application.h"
#include "gui/MainWindow.h"
#include "gui/theme/ThemeManager.h"
#include "media/MediaDeviceManager.h"
#include "sip/SipManager.h"
#include "core/CallHistoryRecorder.h"
#include "core/Logger.h"
#include "core/PerfScope.h"

#include <QTimer>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] QApplication constructed at T+%1 ms")
            .arg(PerfScope::msecsSinceAppStart()));

    setApplicationName("SIP Client");
    setApplicationDisplayName("SIP Client - Audio / Video / RTT");
    setApplicationVersion("0.1.0");
    setOrganizationName("SIPClient");
    setOrganizationDomain("sipclient.local");

    {
        PerfScope s("Application::loadStyleSheet");
        loadStyleSheet();
    }

    Logger::instance().info(LogCategory::App, "Application starting v0.1.0");

    // Start recording call history as soon as SipManager's signals are
    // available. Touching instance() wires the connections; no PJSIP
    // initialization is required for this.
    CallHistoryRecorder::instance();

    {
        PerfScope s("MainWindow constructor");
        m_mainWindow = new MainWindow();
    }

    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] MainWindow constructed at T+%1 ms")
            .arg(PerfScope::msecsSinceAppStart()));

    m_mainWindow->show();

    Logger::instance().info(LogCategory::Perf,
        QStringLiteral("[PERF] MainWindow::show() called at T+%1 ms")
            .arg(PerfScope::msecsSinceAppStart()));

    // Show pending status before deferred init
    m_mainWindow->setInitializingStatus(tr("Initializing SIP backend..."));

    // Defer SIP initialisation to the first event loop tick so the window can
    // paint before PJSIP libCreate/libInit/libStart block the UI thread (~250 ms+).
    QTimer::singleShot(0, this, [this]() {
        Logger::instance().info(LogCategory::Perf,
            QStringLiteral("[PERF] first event loop tick at T+%1 ms")
                .arg(PerfScope::msecsSinceAppStart()));
        {
            PerfScope s("SipManager::initialize");
            SipManager::instance().initialize();
        }
        Logger::instance().info(LogCategory::Perf,
            QStringLiteral("[PERF] SIP backend ready at T+%1 ms")
                .arg(PerfScope::msecsSinceAppStart()));
        m_mainWindow->updateSipBackendStatus();
    });

    // Defer audio device enumeration so the Windows multimedia backend has
    // time to initialise after the event loop starts. Without this, Qt's
    // QMediaDevices::audioInputs() may return an empty list on first call.
    QTimer::singleShot(400, this, [] {
        Logger::instance().info(LogCategory::Perf,
            QStringLiteral("[PERF] media device enumeration started at T+%1 ms")
                .arg(PerfScope::msecsSinceAppStart()));
        MediaDeviceManager::instance().refreshDevices();
    });
}

Application::~Application()
{
    Logger::instance().info(LogCategory::App, "Application shutting down");
    SipManager::instance().shutdown();
    delete m_mainWindow;
}

void Application::loadStyleSheet()
{
    // Apply the theme saved in QSettings (default: Auto = Dark).
    // ThemeManager calls QApplication::setStyleSheet() — no widgets are rebuilt.
    ThemeManager::instance().applyFromSettings();
}
