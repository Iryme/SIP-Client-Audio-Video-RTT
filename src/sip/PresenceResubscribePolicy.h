#pragma once
#include <QString>

// Pure decision logic for Presence auto-resubscribe (Task W098 requirement
// 6), factored out of SipManager/SipAccount so it can be unit tested without
// a live PJSIP stack. Never touches PJSIP or the network itself.
class PresenceResubscribePolicy
{
public:
    // reason must already be normalized (see
    // PresenceInfo::normalizeSubscriptionReason) — one of timeout/
    // deactivated/probation/rejected/noresource/giveup/invariant/unknown.
    // "rejected" and "noresource" mean the peer explicitly does not want us
    // subscribed (or does not exist) — retrying automatically would just
    // hammer the server, so those require an explicit user action instead.
    static bool shouldAutoRetry(const QString &normalizedReason)
    {
        return normalizedReason != QLatin1String("rejected")
            && normalizedReason != QLatin1String("noresource");
    }

    // Exponential backoff, attempt is 1-based (first retry = attempt 1).
    // Capped at kMaxBackoffMs so a long-lived failure never grows unbounded
    // and never creates a tight SUBSCRIBE loop.
    static constexpr int kBaseBackoffMs = 5000;
    static constexpr int kMaxBackoffMs  = 300000;

    static int backoffMs(int attempt)
    {
        if (attempt <= 1)
            return kBaseBackoffMs;
        qint64 delay = kBaseBackoffMs;
        for (int i = 1; i < attempt; ++i) {
            delay *= 2;
            if (delay >= kMaxBackoffMs)
                return kMaxBackoffMs;
        }
        return static_cast<int>(delay);
    }
};
