#pragma once
#include <QList>
#include <QObject>
#include <QSize>
#include "media/VideoSettings.h"

class QCameraDevice;
class QCameraFormat;

class VideoQualityManager : public QObject
{
    Q_OBJECT
public:
    static VideoQualityManager &instance();

    VideoSettings current() const;
    void apply(const VideoSettings &s);

    static QList<QSize>  resolutionsFor(const QString &cameraId);
    static QList<int>    fpsValuesFor(const QString &cameraId, const QSize &resolution);
    static QCameraFormat bestFormat(const QCameraDevice &dev, QSize want, int wantFps);

    static const QList<QSize>  &fallbackResolutions();
    static const QList<int>    &standardFpsValues();
    static const QStringList   &knownCodecs();

signals:
    void settingsChanged(const VideoSettings &s);

private slots:
    void onQualityDegraded();

private:
    VideoQualityManager();

    VideoSettings m_current;
    bool          m_adaptivePending{false};
};
