#include "core/RateLimiter.h"

RateLimiter::RateLimiter(std::int64_t window_ms) : window_ms_(window_ms) {}

bool RateLimiter::Allow(ErrorCode code, std::int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    const int key = static_cast<int>(code);
    const auto it = last_allowed_.find(key);
    if (it != last_allowed_.end() && now_ms - it->second < window_ms_) {
        return false;
    }
    last_allowed_[key] = now_ms;
    return true;
}
