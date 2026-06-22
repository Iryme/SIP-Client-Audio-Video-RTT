#include "Application.h"
#include "gui/MainWindow.h"
#include "diagnostics/DiagnosticsLogger.h"
#include "core/Logger.h"

#include <QFile>
#include <QSysInfo>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName("SIP Client");
    setApplicationDisplayName("SIP Client - Audio / Video / RTT");
    setApplicationVersion("0.1.0");
    setOrganizationName("SIPClient");
    setOrganizationDomain("sipclient.local");

    loadStyleSheet();

    m_mainWindow = new MainWindow();
    m_mainWindow->show();

    // Sample startup logs — emitted after DiagnosticsPanel is connected so they
    // appear in the console. Order matches expected startup sequence.
    DiagnosticsLogger::instance().info(LogCategory::App,      "Application started v0.1.0");
    DiagnosticsLogger::instance().info(LogCategory::Platform, "Platform: " + QSysInfo::prettyProductName());
    DiagnosticsLogger::instance().debug(LogCategory::App,     "Diagnostics console initialized");
}

Application::~Application()
{
    DiagnosticsLogger::instance().info(LogCategory::App, "Application shutting down");
    delete m_mainWindow;
}

void Application::loadStyleSheet()
{
    QFile f(":/styles/dark_theme.qss");
    if (f.open(QFile::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(f.readAll()));
        f.close();
    }
}
