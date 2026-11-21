#pragma once

#include <cstdint>
#include <optional>

namespace lwf {

// Heat-ramp level (0-4 quartiles of the month maximum) for the activity
// calendar widget. Kept as the portable ground truth the HeatLevel widget
// tests pin against (tests/test_overlay_settings_ui.cpp).
int CalendarLevel(std::int64_t day_words, std::int64_t month_max_words);

// Average words per minute over an accumulated duration. Empty below one
// accumulated minute (the floor keeps tiny samples from presenting noisy
// averages); negative word counts clamp to zero. Insights feeds it twice:
// total words over the mic-open speaking time ("words spoken per minute")
// and total words over the end-to-end session time, transcription included
// ("words written per minute").
std::optional<double> Wpm(std::int64_t total_words, std::int64_t duration_ms);

}
