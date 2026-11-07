#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lwf {

struct DayRecord {
    std::string day;
    std::int64_t words = 0;
    std::int64_t speaking_ms = 0;
    std::int64_t fixes = 0;
    std::string app;
    std::string category;
};

std::string DayKey(int year, int month, int day);

std::optional<double> Wpm(std::int64_t total_words, std::int64_t speaking_ms);

std::int64_t FixesMadeByFlow(const std::vector<DayRecord>& records);

std::vector<std::pair<std::string, int>> CategoryShares(const std::vector<DayRecord>& records);

int TotalAppsUsed(const std::vector<DayRecord>& records);

int CurrentStreak(const std::vector<DayRecord>& records, const std::string& today_key);

int LongestStreak(const std::vector<DayRecord>& records);

int CalendarLevel(std::int64_t day_words, std::int64_t month_max_words);

}
