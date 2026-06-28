#include "CameraController.h"
#include "core/Logger.h"

CameraController &CameraController::instance()
{
    static CameraController inst;
    return inst;
}

CameraController::CameraController(QObject *parent)
    : QObject(parent)
{}

void CameraController::setEnabled(bool enabled, const QString &source)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;

    if (enabled) {
        Logger::instance().info(LogCategory::Media,
            source.isEmpty()
                ? QStringLiteral("Camera On requested")
                : QStringLiteral("Camera On requested from %1").arg(source));
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Camera active owner: %1")
                .arg(source.isEmpty() ? QStringLiteral("unknown") : source));
    } else {
        Logger::instance().info(LogCategory::Media,
            source.isEmpty()
                ? QStringLiteral("Camera Off requested")
                : QStringLiteral("Camera Off requested from %1").arg(source));
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Releasing camera from all owners"));
    }

    emit enabledChanged(enabled);

    if (!enabled)
        Logger::instance().info(LogCategory::Media, QStringLiteral("Camera released"));
}
