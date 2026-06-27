#include "VideoQualityManager.h"
#include "media/VideoStatistics.h"
#include "core/Logger.h"

#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaDevices>
#include <QTimer>
#include <algorithm>
#include <climits>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

VideoQualityManager &VideoQualityManager::instance()
{
    static VideoQualityManager s;
    return s;
}

VideoQualityManager::VideoQualityManager()
    : QObject(nullptr)
{
    m_current = VideoSettings::load();

    connect(&VideoStatistics::instance(), &VideoStatistics::qualityDegraded,
            this, &VideoQualityManager::onQualityDegraded);
}

// ---------------------------------------------------------------------------
// Settings access
// ---------------------------------------------------------------------------

VideoSettings VideoQualityManager::current() const
{
    return m_current;
}

void VideoQualityManager::apply(const VideoSettings &s)
{
    bool changed = false;

    if (m_current.cameraId != s.cameraId) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoQualityManager: camera changed to '%1'").arg(s.cameraId));
        changed = true;
    }
    if (m_current.resolution != s.resolution || m_current.fps != s.fps) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoQualityManager: format changed to %1x%2 @ %3 fps")
                .arg(s.resolution.width()).arg(s.resolution.height()).arg(s.fps));
        changed = true;
    }
    if (m_current.bitrateKbps != s.bitrateKbps) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoQualityManager: bitrate changed to %1 kbps").arg(s.bitrateKbps));
        changed = true;
    }
    if (m_current.codecOrder != s.codecOrder) {
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("VideoQualityManager: codec order changed to [%1]")
                .arg(s.codecOrder.join(QLatin1String(", "))));
        changed = true;
    }

    m_current = s;
    s.save();

    if (changed)
        emit settingsChanged(m_current);
}

// ---------------------------------------------------------------------------
// Camera format enumeration
// ---------------------------------------------------------------------------

static QCameraDevice findCameraById(const QString &cameraId)
{
    const QByteArray idBytes = cameraId.toLatin1();
    for (const QCameraDevice &dev : QMediaDevices::videoInputs()) {
        if (dev.id() == idBytes)
            return dev;
    }
    return {};
}

QList<QSize> VideoQualityManager::resolutionsFor(const QString &cameraId)
{
    QList<QSize> result;

    const QCameraDevice dev = findCameraById(cameraId);
    if (!dev.isNull()) {
        for (const QCameraFormat &fmt : dev.videoFormats()) {
            const QSize res = fmt.resolution();
            if (!result.contains(res))
                result.append(res);
        }
    }

    if (result.isEmpty())
        return fallbackResolutions();

    std::sort(result.begin(), result.end(), [](const QSize &a, const QSize &b) {
        return a.width() * a.height() < b.width() * b.height();
    });
    return result;
}

QList<int> VideoQualityManager::fpsValuesFor(const QString &cameraId, const QSize &resolution)
{
    QList<int> supported;

    const QCameraDevice dev = findCameraById(cameraId);
    if (!dev.isNull()) {
        for (const QCameraFormat &fmt : dev.videoFormats()) {
            if (fmt.resolution() == resolution) {
                const int fps = static_cast<int>(fmt.maxFrameRate());
                if (fps > 0 && !supported.contains(fps))
                    supported.append(fps);
            }
        }
    }

    if (supported.isEmpty())
        return standardFpsValues();

    // Return standard fps values that are <= camera's maximum supported fps
    const int maxFps = *std::max_element(supported.begin(), supported.end());
    QList<int> result;
    for (int fps : standardFpsValues()) {
        if (fps <= maxFps && !result.contains(fps))
            result.append(fps);
    }
    return result.isEmpty() ? standardFpsValues() : result;
}

QCameraFormat VideoQualityManager::bestFormat(const QCameraDevice &dev, QSize want, int wantFps)
{
    QCameraFormat best;
    int bestScore = INT_MAX;

    for (const QCameraFormat &fmt : dev.videoFormats()) {
        const QSize res = fmt.resolution();
        const int dr = std::abs(res.width() - want.width())
                     + std::abs(res.height() - want.height());
        const int df = std::abs(static_cast<int>(fmt.maxFrameRate()) - wantFps);
        const int score = dr * 100 + df;
        if (score < bestScore) {
            bestScore = score;
            best = fmt;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Static lists
// ---------------------------------------------------------------------------

const QList<QSize> &VideoQualityManager::fallbackResolutions()
{
    static const QList<QSize> list{
        {320, 240}, {640, 480}, {800, 600},
        {1024, 768}, {1280, 720}, {1920, 1080}, {3840, 2160}
    };
    return list;
}

const QList<int> &VideoQualityManager::standardFpsValues()
{
    static const QList<int> list{15, 20, 24, 25, 30, 50, 60};
    return list;
}

const QStringList &VideoQualityManager::knownCodecs()
{
    static const QStringList list{"H264", "VP8", "VP9", "AV1", "H265"};
    return list;
}

// ---------------------------------------------------------------------------
// Adaptive quality
// ---------------------------------------------------------------------------

void VideoQualityManager::onQualityDegraded()
{
    if (m_adaptivePending)
        return;
    m_adaptivePending = true;

    VideoSettings s = m_current;

    // Step 1: reduce bitrate
    if (s.bitrateKbps > 256) {
        s.bitrateKbps = qMax(256, s.bitrateKbps / 2);
        Logger::instance().info(LogCategory::Media,
            QStringLiteral("Adaptive quality: reducing bitrate to %1 kbps").arg(s.bitrateKbps));
        apply(s);
    } else {
        // Step 2: reduce fps
        const auto fpsList = standardFpsValues();
        const int idx = fpsList.indexOf(s.fps);
        if (idx > 0) {
            s.fps = fpsList[idx - 1];
            Logger::instance().info(LogCategory::Media,
                QStringLiteral("Adaptive quality: reducing fps to %1").arg(s.fps));
            apply(s);
        } else {
            // Step 3: reduce resolution
            const auto resList = fallbackResolutions();
            const int ridx = resList.indexOf(s.resolution);
            if (ridx > 0) {
                s.resolution = resList[ridx - 1];
                Logger::instance().info(LogCategory::Media,
                    QStringLiteral("Adaptive quality: reducing resolution to %1x%2")
                        .arg(s.resolution.width()).arg(s.resolution.height()));
                apply(s);
            }
        }
    }

    QTimer::singleShot(10000, this, [this]() { m_adaptivePending = false; });
}
