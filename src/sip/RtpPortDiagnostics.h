#pragma once
#include <QString>

// Task W109A — classifies a PJSIP/OS error reason string (as surfaced by
// pj::Error::reason, e.g. from a failed reinvite() when the RTP transport
// could not bind) into a distinct, non-sensitive diagnostic category. Used so
// "the RTP port range is exhausted/colliding with another process" is logged
// and surfaced to the UI as a clearly different condition from a generic
// negotiation failure, instead of a single vague "requestRtt() PJSIP error"
// line that is indistinguishable from unrelated failures.
enum class RtpPortErrorKind {
    NotPortRelated,      // Reason text does not look port/bind related.
    RtpPortInUse,        // A specific port is already bound (WSAEADDRINUSE / EADDRINUSE).
    RtpPortRangeExhausted, // No free port found anywhere in the configured range.
    RtpPortAllocationFailed // Some other bind/transport-creation failure.
};

QString rtpPortErrorKindName(RtpPortErrorKind kind);

// Never throws; matches on substrings only, case-insensitive. `reason` is
// expected to be pj::Error::reason (already a plain, non-sensitive string —
// no credentials/URIs are ever part of a bind-failure reason).
RtpPortErrorKind classifyRtpPortError(const QString &reason);
