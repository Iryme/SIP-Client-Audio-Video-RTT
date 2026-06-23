#pragma once

struct RegistrationRetryPolicy {
    int    maxAttempts{5};
    int    initialDelayMs{2000};
    int    maxDelayMs{60000};
    double multiplier{2.0};

    // Returns true for status codes that indicate a transient failure worth retrying:
    // 0 (no response/timeout), 408 (Request Timeout), 5xx (Server Error).
    // Returns false for 4xx auth/config failures that require user action (401, 403, 404…).
    static bool isRetryable(int statusCode);

    // Exponential backoff: initialDelayMs * multiplier^(attempt-1), capped at maxDelayMs.
    // attempt is 1-based (first retry = attempt 1).
    int delayForAttempt(int attempt) const;
};
