#include "RegistrationRefreshConfig.h"

#include <algorithm>

int RegistrationRefreshConfig::delayMsForExpiry(int expirySeconds) const
{
    if (overrideDelayMs >= 0)
        return overrideDelayMs;

    const int effective = (expirySeconds > 0) ? expirySeconds : defaultExpirySeconds;

    // Fire at whichever comes first: the ratio point or the margin point.
    int delaySeconds = std::min(
        static_cast<int>(static_cast<double>(effective) * refreshRatio),
        effective - minMarginSeconds
    );

    // Guard: if both approaches leave no time (very short expiry), use half-expiry.
    if (delaySeconds <= 0)
        delaySeconds = std::max(1, effective / 2);

    return delaySeconds * 1000;
}
