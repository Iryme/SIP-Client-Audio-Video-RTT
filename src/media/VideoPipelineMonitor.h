#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QString>

class VideoPipelineMonitor : public QObject
{
    Q_OBJECT
public:
    enum class Stage { Capture, Encode, Transmit, Receive, Decode, Render };
    Q_ENUM(Stage)
    static constexpr int kStageCount = 6;

    static VideoPipelineMonitor &instance();

    void    stageBegin(Stage s);
    void    stageEnd(Stage s);
    qint64  latencyMs(Stage s) const;
    QString stageName(Stage s) const;

signals:
    void stageTimingUpdated(Stage s, qint64 ms);

private:
    VideoPipelineMonitor();

    struct StageStat {
        QElapsedTimer timer;
        qint64        lastMs{0};
        bool          running{false};
    };
    StageStat m_stages[kStageCount];
};
