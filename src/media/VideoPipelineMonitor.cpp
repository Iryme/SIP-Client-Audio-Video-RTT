#include "VideoPipelineMonitor.h"

VideoPipelineMonitor &VideoPipelineMonitor::instance()
{
    static VideoPipelineMonitor s;
    return s;
}

VideoPipelineMonitor::VideoPipelineMonitor()
    : QObject(nullptr)
{}

void VideoPipelineMonitor::stageBegin(Stage s)
{
    const int idx = static_cast<int>(s);
    m_stages[idx].running = true;
    m_stages[idx].timer.start();
}

void VideoPipelineMonitor::stageEnd(Stage s)
{
    const int idx = static_cast<int>(s);
    if (!m_stages[idx].running)
        return;
    m_stages[idx].lastMs  = m_stages[idx].timer.elapsed();
    m_stages[idx].running = false;
    emit stageTimingUpdated(s, m_stages[idx].lastMs);
}

qint64 VideoPipelineMonitor::latencyMs(Stage s) const
{
    return m_stages[static_cast<int>(s)].lastMs;
}

QString VideoPipelineMonitor::stageName(Stage s) const
{
    switch (s) {
    case Stage::Capture:  return QStringLiteral("Capture");
    case Stage::Encode:   return QStringLiteral("Encode");
    case Stage::Transmit: return QStringLiteral("Transmit");
    case Stage::Receive:  return QStringLiteral("Receive");
    case Stage::Decode:   return QStringLiteral("Decode");
    case Stage::Render:   return QStringLiteral("Render");
    }
    return QStringLiteral("?");
}
