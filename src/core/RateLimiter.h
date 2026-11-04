#pragma once

#include "core/ErrorCode.h"

#include <mutex>
#include <unordered_map>

class RateLimiter {
public:
    explicit RateLimiter(std::int64_t window_ms = 10000);

    bool Allow(ErrorCode code, std::int64_t now_ms);

private:
    std::int64_t window_ms_;
    mutable std::mutex mutex_;
    std::unordered_map<int, std::int64_t> last_allowed_;
};
