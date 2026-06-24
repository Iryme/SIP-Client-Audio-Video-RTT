#include "Application.h"
#include "gui/MainWindow.h"
#include "media/MediaDeviceManager.h"
#include "sip/SipManager.h"
#include "core/Logger.h"

#include <QFile>
#include <QStyleFactory>
#include <QTimer>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName("SIP Client");
    setApplicationDisplayName("SIP Client - Audio / Video / RTT");
    setApplicationVersion("0.1.0");
    setOrganizationName("SIPClient");
    setOrganizationDomain("sipclient.local");

    loadStyleSheet();

    Logger::instance().info(LogCategory::App, "Application starting v0.1.0");

    m_mainWindow = new MainWindow();
    m_mainWindow->show();

    SipManager::instance().initialize();
    m_mainWindow->updateSipBackendStatus();

    // Defer audio device enumeration so the Windows multimedia backend has
    // time to initialise after the event loop starts. Without this, Qt's
    // QMediaDevices::audioInputs() may return an empty list on first call.
    QTimer::singleShot(400, this, [] {
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
    QFile f(":/styles/dark_theme.qss");
    if (f.open(QFile::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(f.readAll()));
        f.close();
    }
}
