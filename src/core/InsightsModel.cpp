#include "core/InsightsModel.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <unordered_map>

namespace lwf {
namespace {

std::int64_t DaysFromCivil(int year, int month, int day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yoe = year - era * 400;
    const std::int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

bool ParseDayKey(const std::string& day, std::int64_t& serial) {
    if (day.size() != 10 || day[4] != '-' || day[7] != '-') {
        return false;
    }
    int year = 0;
    int month = 0;
    int day_of_month = 0;
    for (std::size_t k = 0; k < 4; ++k) {
        if (day[k] < '0' || day[k] > '9') {
            return false;
        }
        year = year * 10 + (day[k] - '0');
    }
    for (std::size_t k = 5; k < 7; ++k) {
        if (day[k] < '0' || day[k] > '9') {
            return false;
        }
        month = month * 10 + (day[k] - '0');
    }
    for (std::size_t k = 8; k < 10; ++k) {
        if (day[k] < '0' || day[k] > '9') {
            return false;
        }
        day_of_month = day_of_month * 10 + (day[k] - '0');
    }
    if (month < 1 || month > 12 || day_of_month < 1 || day_of_month > 31) {
        return false;
    }
    static const int kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_day = kDaysInMonth[month - 1];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        max_day = 29;
    }
    if (day_of_month > max_day) {
        return false;
    }
    serial = DaysFromCivil(year, month, day_of_month);
    return true;
}

std::set<std::int64_t> ActiveSerials(const std::vector<DayRecord>& records) {
    std::unordered_map<std::string, std::int64_t> words_by_day;
    for (const DayRecord& record : records) {
        if (record.words > 0) {
            words_by_day[record.day] += record.words;
        }
    }
    std::set<std::int64_t> serials;
    for (const auto& [day, words] : words_by_day) {
        std::int64_t serial = 0;
        if (words > 0 && ParseDayKey(day, serial)) {
            serials.insert(serial);
        }
    }
    return serials;
}

}

std::string DayKey(int year, int month, int day) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    return std::string(buffer);
}

std::optional<double> Wpm(std::int64_t total_words, std::int64_t speaking_ms) {
    if (speaking_ms < 60000) {
        return std::nullopt;
    }
    const std::int64_t words = total_words > 0 ? total_words : 0;
    return static_cast<double>(words) * 60000.0 / static_cast<double>(speaking_ms);
}

std::int64_t FixesMadeByFlow(const std::vector<DayRecord>& records) {
    std::int64_t total = 0;
    for (const DayRecord& record : records) {
        if (record.fixes > 0) {
            total += record.fixes;
        }
    }
    return total;
}

std::vector<std::pair<std::string, int>> CategoryShares(const std::vector<DayRecord>& records) {
    std::unordered_map<std::string, std::int64_t> words_by_category;
    std::int64_t total_words = 0;
    for (const DayRecord& record : records) {
        if (record.words > 0) {
            words_by_category[record.category] += record.words;
            total_words += record.words;
        }
    }

    std::vector<std::pair<std::string, int>> shares;
    if (total_words <= 0) {
        return shares;
    }

    shares.reserve(words_by_category.size());
    for (const auto& [category, words] : words_by_category) {
        shares.push_back({category, static_cast<int>(words * 100 / total_words)});
    }
    std::sort(shares.begin(), shares.end(),
              [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                  if (a.second != b.second) {
                      return a.second > b.second;
                  }
                  return a.first < b.first;
              });

    int distributed = 0;
    for (const auto& share : shares) {
        distributed += share.second;
    }
    int remainder = 100 - distributed;
    for (std::size_t k = 0; k < shares.size() && remainder > 0; ++k) {
        shares[k].second += 1;
        --remainder;
    }

    return shares;
}

int TotalAppsUsed(const std::vector<DayRecord>& records) {
    std::set<std::string> apps;
    for (const DayRecord& record : records) {
        if (!record.app.empty()) {
            apps.insert(record.app);
        }
    }
    return static_cast<int>(apps.size());
}

int CurrentStreak(const std::vector<DayRecord>& records, const std::string& today_key) {
    std::int64_t today = 0;
    if (!ParseDayKey(today_key, today)) {
        return 0;
    }
    const std::set<std::int64_t> active = ActiveSerials(records);
    if (active.find(today) == active.end()) {
        return 0;
    }
    int streak = 0;
    std::int64_t day = today;
    while (active.find(day) != active.end()) {
        ++streak;
        --day;
    }
    return streak;
}

int LongestStreak(const std::vector<DayRecord>& records) {
    const std::set<std::int64_t> active = ActiveSerials(records);
    int longest = 0;
    int run = 0;
    std::int64_t previous = 0;
    for (std::int64_t day : active) {
        if (run > 0 && day == previous + 1) {
            ++run;
        } else {
            run = 1;
        }
        previous = day;
        if (run > longest) {
            longest = run;
        }
    }
    return longest;
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
