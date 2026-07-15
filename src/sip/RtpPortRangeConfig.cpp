#include "sip/RtpPortRangeConfig.h"
#include "core/AppSettings.h"

namespace {
// Minimum ports needed to comfortably run one call's audio+video+text RTP/RTCP
// pairs (3 streams * 2 ports each = 6) plus headroom for the OS reusing a
// just-closed port during hold/resume or back-to-back calls.
constexpr int kMinUsablePorts = 16;
constexpr int kRecommendedPorts = 40;
constexpr int kMinPort = 1024;
constexpr int kMaxPort = 65535;

int  g_sessionOverrideStart = 0;
int  g_sessionOverrideEnd   = 0;
}

bool validateRtpPortRange(int start, int end, QString *error, QString *warning)
{
    if (warning)
        warning->clear();

    if (start < kMinPort || start > kMaxPort) {
        if (error) *error = QStringLiteral("start port %1 out of range [%2-%3]")
            .arg(start).arg(kMinPort).arg(kMaxPort);
        return false;
    }
    if (end < kMinPort || end > kMaxPort) {
        if (error) *error = QStringLiteral("end port %1 out of range [%2-%3]")
            .arg(end).arg(kMinPort).arg(kMaxPort);
        return false;
    }
    if (start > end) {
        if (error) *error = QStringLiteral("start port %1 must be <= end port %2")
            .arg(start).arg(end);
        return false;
    }
    if ((start % 2) != 0) {
        if (error) *error = QStringLiteral("start port %1 must be even "
            "(RTCP is allocated at RTP port + 1)").arg(start);
        return false;
    }

    const int available = end - start + 1;
    if (available < kMinUsablePorts) {
        if (error) *error = QStringLiteral(
            "range [%1-%2] only has %3 ports; at least %4 are required for "
            "audio+video+text RTP/RTCP pairs plus headroom")
            .arg(start).arg(end).arg(available).arg(kMinUsablePorts);
        return false;
    }
    if (available < kRecommendedPorts && warning) {
        *warning = QStringLiteral(
            "range [%1-%2] has only %3 ports; %4+ is recommended for headroom "
            "across repeated calls/hold-resume without port reuse pressure")
            .arg(start).arg(end).arg(available).arg(kRecommendedPorts);
    }

    if (error) error->clear();
    return true;
}

void setRtpPortRangeSessionOverride(int start, int end)
{
    if (start <= 0 || end <= 0) {
        g_sessionOverrideStart = 0;
        g_sessionOverrideEnd   = 0;
        return;
    }
    g_sessionOverrideStart = start;
    g_sessionOverrideEnd   = end;
}

bool hasRtpPortRangeSessionOverride()
{
    return g_sessionOverrideStart > 0 && g_sessionOverrideEnd > 0;
}

RtpPortRangeConfig resolveEffectiveRtpPortRange()
{
    RtpPortRangeConfig cfg;
    if (hasRtpPortRangeSessionOverride()) {
        cfg.start = g_sessionOverrideStart;
        cfg.end   = g_sessionOverrideEnd;
        return cfg;
    }
    cfg.start = AppSettings::rtpPortRangeStart();
    cfg.end   = AppSettings::rtpPortRangeEnd();
    return cfg;
}
