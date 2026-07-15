#pragma once
#include <QString>

// Task W109A — configurable local RTP/RTCP media port range, so that two
// instances of this application running on the same host (e.g. two test
// clients on one Windows machine) can each be given a distinct, non-
// overlapping range and never collide on the same UDP port (WSAEADDRINUSE).
//
// Applied via pj::AccountConfig::mediaConfig::transportConfig (port/portRange)
// at account-creation time in SipAccount::startRegistration() — this is the
// actual pjsua2 API surface for the per-account RTP/RTCP port range (audio,
// video, and text streams all share it); pj::EpConfig::MediaConfig has no
// port-range field at all (verified against the vendored pjsua2 headers in
// .deps/pjproject/pjsip/include/pjsua2/endpoint.hpp).
struct RtpPortRangeConfig
{
    int start{4000};
    int end{4998};

    int portRange() const { return end - start; }
};

// Validates a candidate range. Returns true when usable; on false, `error`
// explains why (e.g. "start must be <= end"). Regardless of the return value,
// `warning` may be set to a non-fatal concern (e.g. "range is small; may not
// fit audio+video+text simultaneously") that callers should log but need not
// treat as blocking.
bool validateRtpPortRange(int start, int end, QString *error, QString *warning = nullptr);

// Resolves the effective range for this process, in priority order:
//   1. an in-process override set via RtpPortRangeConfig::setSessionOverride()
//      (used for --rtp-port-start/--rtp-port-end command-line arguments)
//   2. AppSettings::rtpPortRangeStart()/rtpPortRangeEnd()
//   3. the struct defaults above (matches the range pjsua2 would otherwise
//      use unbounded from its own built-in default start port)
RtpPortRangeConfig resolveEffectiveRtpPortRange();

// Session-only (never persisted) override, e.g. from --rtp-port-start/--rtp-
// port-end. Passing start<=0 or end<=0 clears any existing override.
void setRtpPortRangeSessionOverride(int start, int end);
bool hasRtpPortRangeSessionOverride();
