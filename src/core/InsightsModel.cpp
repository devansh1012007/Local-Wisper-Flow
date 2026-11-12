#include "core/InsightsModel.h"

namespace lwf {

std::optional<double> Wpm(std::int64_t total_words, std::int64_t speaking_ms) {
    if (speaking_ms < 60000) {
        return std::nullopt;
    }
    const std::int64_t words = total_words > 0 ? total_words : 0;
    return static_cast<double>(words) * 60000.0 / static_cast<double>(speaking_ms);
}

int CalendarLevel(std::int64_t day_words, std::int64_t month_max_words) {
    if (day_words <= 0 || month_max_words <= 0) {
        return 0;
    }
    if (day_words >= month_max_words) {
        return 4;
    }
    const std::int64_t quarter = day_words * 4 / month_max_words;
    return static_cast<int>(quarter > 0 ? quarter : 1);
}

}
