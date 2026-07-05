#pragma once

#include <QString>
#include <QMetaType>

// Snapshot of RTP/RTCP media statistics for the active call.
// Values are reported only when the backend can read real PJSIP stats.
struct RtpStatsSnapshot
{
    bool    available{false};
    QString source;
    QString reason;

    bool    jitterAvailable{false};
    double  jitterMs{0.0};

    bool    packetLossAvailable{false};
    double  packetLossPercent{0.0};
    unsigned packetLossPackets{0};
    unsigned packetReceivedPackets{0};

    bool    packetsTxAvailable{false};
    unsigned packetsTx{0};

    bool    rttAvailable{false};
    double  rttMs{0.0};

    int     streamIndex{-1};
    QString streamType;
};

Q_DECLARE_METATYPE(RtpStatsSnapshot)
