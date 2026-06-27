#include "VideoStatistics.h"

VideoStatistics &VideoStatistics::instance()
{
    static VideoStatistics s;
    return s;
}

VideoStatistics::VideoStatistics()
    : QObject(nullptr)
{
    m_ticker.setInterval(1000);
    connect(&m_ticker, &QTimer::timeout, this, &VideoStatistics::tick);
    m_ticker.start();
    m_elapsed.start();
}

void VideoStatistics::frameProduced()
{
    ++m_frameCount;
}

void VideoStatistics::frameDrop()
{
    ++m_dropsThisSec;
    ++m_totalDrops;
}

void VideoStatistics::reset()
{
    m_frameCount    = 0;
    m_dropsThisSec  = 0;
    m_totalDrops    = 0;
    m_fps           = 0.f;
    m_lowFpsSeconds = 0;
    m_elapsed.restart();
}

void VideoStatistics::tick()
{
    const qint64 ms = m_elapsed.restart();
    m_fps = ms > 0 ? static_cast<float>(m_frameCount) * 1000.f / static_cast<float>(ms) : 0.f;

    const int drops = m_dropsThisSec;
    m_frameCount   = 0;
    m_dropsThisSec = 0;

    emit statsUpdated(m_fps, drops);

    if (m_fps > 0.f && m_fps < kLowFpsThreshold) {
        ++m_lowFpsSeconds;
        if (m_lowFpsSeconds >= kLowFpsGrace) {
            m_lowFpsSeconds = 0;
            emit qualityDegraded();
        }
    } else {
        m_lowFpsSeconds = 0;
    }
}
