#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class VideoStatistics : public QObject
{
    Q_OBJECT
public:
    static VideoStatistics &instance();

    void frameProduced();
    void frameDrop();
    void reset();

    float currentFps()    const { return m_fps; }
    int   dropsThisSec()  const { return m_dropsThisSec; }
    int   totalDrops()    const { return m_totalDrops; }

signals:
    void statsUpdated(float fps, int dropsThisSec);
    void qualityDegraded();

private:
    VideoStatistics();
    void tick();

    QTimer        m_ticker;
    QElapsedTimer m_elapsed;
    int           m_frameCount{0};
    int           m_dropsThisSec{0};
    int           m_totalDrops{0};
    float         m_fps{0.f};
    int           m_lowFpsSeconds{0};

    static constexpr float kLowFpsThreshold = 15.f;
    static constexpr int   kLowFpsGrace     = 3;
};
