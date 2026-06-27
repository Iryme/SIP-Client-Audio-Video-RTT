#include "VideoSettings.h"
#include "core/AppSettings.h"

VideoSettings VideoSettings::load()
{
    auto &s = AppSettings::settings();
    VideoSettings vs;
    vs.cameraId      = s.value(QStringLiteral("video/cameraId")).toString();
    vs.resolution    = QSize(
        s.value(QStringLiteral("video/width"),  1280).toInt(),
        s.value(QStringLiteral("video/height"),  720).toInt());
    vs.fps           = s.value(QStringLiteral("video/fps"),         30).toInt();
    vs.bitrateKbps   = s.value(QStringLiteral("video/bitrateKbps"), 1024).toInt();
    vs.overlayEnabled = s.value(QStringLiteral("video/overlayEnabled"), false).toBool();

    const QStringList saved = s.value(QStringLiteral("video/codecOrder")).toStringList();
    if (!saved.isEmpty())
        vs.codecOrder = saved;
    return vs;
}

void VideoSettings::save() const
{
    auto &s = AppSettings::settings();
    s.setValue(QStringLiteral("video/cameraId"),       cameraId);
    s.setValue(QStringLiteral("video/width"),          resolution.width());
    s.setValue(QStringLiteral("video/height"),         resolution.height());
    s.setValue(QStringLiteral("video/fps"),            fps);
    s.setValue(QStringLiteral("video/bitrateKbps"),    bitrateKbps);
    s.setValue(QStringLiteral("video/codecOrder"),     codecOrder);
    s.setValue(QStringLiteral("video/overlayEnabled"), overlayEnabled);
    s.sync();
}
