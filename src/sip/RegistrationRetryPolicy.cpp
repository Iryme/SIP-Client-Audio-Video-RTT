#include "RegistrationRetryPolicy.h"

#include <algorithm>
#include <cmath>

bool RegistrationRetryPolicy::isRetryable(int statusCode)
{
    return statusCode == 0
        || statusCode == 408
        || (statusCode >= 500 && statusCode <= 599);
}

int RegistrationRetryPolicy::delayForAttempt(int attempt) const
{
    const double raw = initialDelayMs * std::pow(multiplier, attempt - 1);
    return static_cast<int>(std::min(raw, static_cast<double>(maxDelayMs)));
}
