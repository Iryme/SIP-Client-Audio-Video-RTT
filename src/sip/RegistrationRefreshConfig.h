#pragma once

struct RegistrationRefreshConfig {
    int    defaultExpirySeconds{300}; // fallback when server provides no expiry
    double refreshRatio{0.80};        // refresh at this fraction of expiry
    int    minMarginSeconds{30};      // minimum seconds before expiry to guarantee
    int    overrideDelayMs{-1};       // >= 0: bypass formula entirely (for tests)

    // Returns the refresh timer delay in milliseconds.
    // If overrideDelayMs >= 0, returns it directly.
    // Otherwise: min(expiry * refreshRatio, expiry - minMarginSeconds), floored at
    // expiry/2 when both approaches yield <= 0, then converted to ms.
    int delayMsForExpiry(int expirySeconds) const;
};
