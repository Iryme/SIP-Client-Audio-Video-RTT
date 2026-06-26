#pragma once

// RTT T.140 + RED (RFC 4103 / RFC 2198) compile-time configuration.
//
// These constants are applied to pj::AccountConfig.textConfig.redundancyLevel
// at account creation time in SipAccount::startRegistration().
// PJSIP enforces an upper bound of PJMEDIA_TXT_STREAM_MAX_RED_LEVELS=2.
//
// The actual redundancy level used in a call is subject to SDP negotiation:
// if the remote peer does not advertise "red/90000" in m=text, PJSIP falls
// back automatically to plain "t140/1000".

// Default redundancy level: 2 (RFC 4103 recommendation).
// Offers protection against up to ~66.7% packet loss at the cost of ~3×
// bandwidth. PJSIP includes both "red/90000" and "t140/1000" in the SDP
// m=text offer when this is > 0.
inline constexpr int kRttRedLevelDefault = 2;

// Value that disables RED entirely (plain t140 only).
// Use when: testing interoperability with non-RED peers, or when bandwidth
// is severely constrained and the network is expected to be lossless.
inline constexpr int kRttRedLevelDisabled = 0;

// Maximum redundancy level supported by this build of PJSIP.
// Equals PJMEDIA_TXT_STREAM_MAX_RED_LEVELS from pjmedia/config.h.
inline constexpr int kRttRedLevelMax = 2;
