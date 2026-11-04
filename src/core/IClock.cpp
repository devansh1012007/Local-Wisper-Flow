#include "core/IClock.h"

#include <chrono>

std::int64_t RealClock::NowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
