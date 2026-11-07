#pragma once

#include "core/InsightsModel.h"
#include "core/StatsStore.h"
#include "ui/theme/StudioPalette.h"
#include "ui/widgets/Overlays.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lwf::ui::insights {

inline constexpr const char* kPageTitle = "Insights";
inline constexpr const char* kTabUsage = "Your usage";
inline constexpr const char* kTabVoice = "Your voice";
inline constexpr const char* kVoiceEmpty = "Not available yet.";
inline constexpr const char* kWpmCaption = "WORDS PER MINUTE";
inline constexpr const char* kWpmTooltip = "Your average speaking speed while dictating.";
inline constexpr const char* kGaugeCenter = "Average";
inline constexpr const char* kGaugeFootnote =
    "Ranking against other users needs an account, which this app does not have.";
inline constexpr const char* kFixesCaption = "FIXES MADE BY FLOW";
inline constexpr const char* kWordsCorrectedSuffix = "words corrected";
inline constexpr const char* kDictionaryFixesSuffix = "dictionary fixes";
inline constexpr const char* kWordsCorrectedTooltip = "Words the cleanup changed.";
inline constexpr const char* kDictionaryFixesTooltip = "Words the dictionary corrected.";
inline constexpr const char* kTotalWordsCaption = "TOTAL WORDS DICTATED";
inline constexpr const char* kDesktopLabel = "Desktop";
inline constexpr const char* kMobileLabel = "Download on mobile";
inline constexpr const char* kMobileTooltip = "Mobile is not available in this app.";
inline constexpr const char* kUsageTitle = "Desktop usage";
inline constexpr const char* kDesktopEmpty = "No dictations yet. Hold Ctrl + Win to start.";
inline constexpr const char* kShareDialogTitle = "Share";
inline constexpr const char* kShareHighlightsTitle = "Flow Highlights";
inline constexpr const char* kShareStreakTitle = "Flow Streak";
inline constexpr const char* kShareCopy = "Copy image";
inline constexpr const char* kShareDownload = "Download image";
inline constexpr const char* kComingSoon = "Coming soon";
inline constexpr const char* kEmptyWpm = "—";
inline constexpr const char* kShareBadgeLabel = "SHARE";
inline constexpr double kWpmGaugeCap = 200.0;

struct CategoryDef {
    const char* key;
    const char* display_label;
    const char* icon;
};

inline std::vector<CategoryDef> CategoryDefs() {
    return {{"AI prompts", "AI PROMPTS", "AI"},
            {"Emails", "EMAILS", "EM"},
            {"Work messages", "WORK MESSAGES", "WK"},
            {"Personal", "PERSONAL MESSAGES", "PM"},
            {"Documents", "DOCUMENTS", "DC"},
            {"Other tasks", "OTHER TASKS", "OT"}};
}

inline std::string DisplayLabelForCategory(const std::string& key) {
    for (const CategoryDef& def : CategoryDefs()) {
        if (key == def.key) {
            return def.display_label;
        }
    }
    return {};
}

inline std::string IconForCategory(const std::string& key) {
    for (const CategoryDef& def : CategoryDefs()) {
        if (key == def.key) {
            return def.icon;
        }
    }
    return {};
}

inline std::string PillLabel(std::int64_t words, const std::string& display_label) {
    return std::to_string(words) + " " + display_label;
}

inline std::string WpmLabel(const std::optional<double>& wpm) {
    if (!wpm.has_value()) {
        return kEmptyWpm;
    }
    return std::to_string(static_cast<long long>(std::llround(*wpm)));
}

inline float GaugeFraction(const std::optional<double>& wpm) {
    if (!wpm.has_value()) {
        return 0.0f;
    }
    const double fraction = *wpm / kWpmGaugeCap;
    if (fraction < 0.0) {
        return 0.0f;
    }
    if (fraction > 1.0) {
        return 1.0f;
    }
    return static_cast<float>(fraction);
}

inline std::string TotalAppsCaption(int apps) {
    return std::string("TOTAL APPS USED | ") + std::to_string(apps);
}

inline std::string LongestCaption(int days) {
    return std::string("LONGEST STREAK | ") + std::to_string(days) + " DAYS";
}

inline std::string StreakTitle(int days) {
    return std::to_string(days) + " day streak";
}

inline std::string DesktopWordsLine(std::int64_t words) {
    return std::string(kDesktopLabel) + " / " + std::to_string(words) + " words";
}

inline std::int64_t DaysFromCivil(int year, int month, int day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yoe = year - era * 400;
    const std::int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

inline int DaysInMonth(int year, int month) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return kDays[month - 1];
}

inline int WeekdayOfFirst(int year, int month) {
    const std::int64_t days = DaysFromCivil(year, month, 1);
    std::int64_t weekday = (days + 4) % 7;
    if (weekday < 0) {
        weekday += 7;
    }
    return static_cast<int>(weekday);
}

inline std::string TodayKeyForTime(std::int64_t timestamp_local) {
    return StatsStore::DateKey(timestamp_local);
}

inline std::string TodayKeyNow() {
    const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
    return StatsStore::DateKey(now);
}

inline std::string MonthKeyFor(int year, int month) {
    return lwf::DayKey(year, month, 1).substr(0, 7);
}

inline bool ParseMonthKey(const std::string& text, int& year, int& month) {
    if (text.size() != 7 || text[4] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4) {
            continue;
        }
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    year = std::stoi(text.substr(0, 4));
    month = std::stoi(text.substr(5, 2));
    return month >= 1 && month <= 12;
}

inline std::string MonthLabelFor(const std::string& month_key) {
    static const char* const kMonths[] = {"January", "February", "March", "April", "May", "June",
                                          "July", "August", "September", "October", "November",
                                          "December"};
    int year = 0;
    int month = 0;
    if (!ParseMonthKey(month_key, year, month)) {
        return {};
    }
    return std::string(kMonths[month - 1]) + " " + std::to_string(year);
}

inline std::int64_t Clamped(std::int64_t value) {
    return value < 0 ? 0 : value;
}

inline std::vector<lwf::DayRecord> BuildDayRecords(
    const std::map<std::string, DayStats>& snapshot) {
    std::vector<lwf::DayRecord> records;
    records.reserve(snapshot.size());
    for (const auto& entry : snapshot) {
        lwf::DayRecord record;
        record.day = entry.first;
        record.words = Clamped(entry.second.words);
        record.speaking_ms = Clamped(entry.second.speaking_ms);
        record.fixes = Clamped(entry.second.words_corrected) + Clamped(entry.second.dictionary_fixes);
        records.push_back(record);
    }
    return records;
}

inline std::vector<lwf::DayRecord> BuildCategoryRecords(
    const std::map<std::string, DayStats>& snapshot) {
    std::vector<lwf::DayRecord> records;
    for (const auto& entry : snapshot) {
        for (const auto& category : entry.second.by_category) {
            const std::int64_t words = Clamped(category.second);
            if (words > 0) {
                lwf::DayRecord record;
                record.day = entry.first;
                record.words = words;
                record.category = category.first;
                records.push_back(record);
            }
        }
    }
    return records;
}

inline std::vector<lwf::DayRecord> BuildAppRecords(
    const std::map<std::string, DayStats>& snapshot) {
    std::vector<lwf::DayRecord> records;
    for (const auto& entry : snapshot) {
        for (const auto& app : entry.second.apps) {
            const std::int64_t words = Clamped(app.second);
            if (words > 0 && !app.first.empty()) {
                lwf::DayRecord record;
                record.day = entry.first;
                record.words = words;
                record.app = app.first;
                records.push_back(record);
            }
        }
    }
    return records;
}

struct InsightsSummary {
    std::int64_t total_words = 0;
    std::int64_t total_speaking_ms = 0;
    std::int64_t words_corrected = 0;
    std::int64_t dictionary_fixes = 0;
    std::int64_t fixes_total = 0;
    int total_apps = 0;
    int current_streak = 0;
    int longest_streak = 0;
    std::optional<double> wpm;
    bool has_data = false;
};

inline InsightsSummary SummarizeSnapshot(const std::map<std::string, DayStats>& snapshot,
                                        const std::string& today_key) {
    InsightsSummary summary;
    for (const auto& entry : snapshot) {
        summary.total_words += Clamped(entry.second.words);
        summary.total_speaking_ms += Clamped(entry.second.speaking_ms);
        summary.words_corrected += Clamped(entry.second.words_corrected);
        summary.dictionary_fixes += Clamped(entry.second.dictionary_fixes);
    }
    const std::vector<lwf::DayRecord> day_records = BuildDayRecords(snapshot);
    summary.fixes_total = lwf::FixesMadeByFlow(day_records);
    summary.wpm = lwf::Wpm(summary.total_words, summary.total_speaking_ms);
    summary.current_streak = lwf::CurrentStreak(day_records, today_key);
    summary.longest_streak = lwf::LongestStreak(day_records);
    summary.total_apps = lwf::TotalAppsUsed(BuildAppRecords(snapshot));
    summary.has_data = summary.total_words > 0;
    return summary;
}

struct CategoryRow {
    std::string key;
    std::string display_label;
    std::string icon;
    std::int64_t words = 0;
    int share = 0;
    float fraction = 0.0f;
};

inline std::vector<CategoryRow> CategoryRowsFor(const std::map<std::string, DayStats>& snapshot) {
    const std::vector<CategoryDef> defs = CategoryDefs();
    std::unordered_map<std::string, std::int64_t> words_by_key;
    std::vector<lwf::DayRecord> filtered;
    std::int64_t total = 0;
    for (const auto& entry : snapshot) {
        for (const CategoryDef& def : defs) {
            const auto found = entry.second.by_category.find(def.key);
            if (found == entry.second.by_category.end()) {
                continue;
            }
            const std::int64_t words = Clamped(found->second);
            if (words > 0) {
                words_by_key[def.key] += words;
                total += words;
                lwf::DayRecord record;
                record.day = entry.first;
                record.words = words;
                record.category = def.key;
                filtered.push_back(record);
            }
        }
    }
    std::unordered_map<std::string, int> share_by_key;
    for (const auto& share : lwf::CategoryShares(filtered)) {
        share_by_key[share.first] = share.second;
    }
    std::vector<CategoryRow> rows;
    rows.reserve(defs.size());
    for (const CategoryDef& def : defs) {
        CategoryRow row;
        row.key = def.key;
        row.display_label = def.display_label;
        row.icon = def.icon;
        const auto found = words_by_key.find(row.key);
        row.words = found == words_by_key.end() ? 0 : found->second;
        const auto share_found = share_by_key.find(row.key);
        row.share = share_found == share_by_key.end() ? 0 : share_found->second;
        row.fraction =
            total > 0 ? static_cast<float>(static_cast<double>(row.words) / static_cast<double>(total)) : 0.0f;
        rows.push_back(row);
    }
    return rows;
}

struct MonthValues {
    int year = 0;
    int month = 0;
    std::vector<int> values;
    int month_max = 0;
};

inline MonthValues MonthValuesFor(const std::map<std::string, DayStats>& snapshot, int year,
                                  int month) {
    MonthValues grid;
    grid.year = year;
    grid.month = month;
    const int days = DaysInMonth(year, month);
    const int offset = WeekdayOfFirst(year, month);
    if (days <= 0) {
        grid.values.assign(1, 0);
        grid.month_max = 0;
        return grid;
    }
    grid.values.assign(static_cast<std::size_t>(offset + days), 0);
    const std::string prefix = MonthKeyFor(year, month);
    for (const auto& entry : snapshot) {
        if (entry.first.size() < 10 || entry.first.compare(0, 7, prefix) != 0 || entry.first[7] != '-') {
            continue;
        }
        const std::string day_text = entry.first.substr(8, 2);
        const bool valid_day = day_text[0] >= '0' && day_text[0] <= '9' && day_text[1] >= '0' &&
                               day_text[1] <= '9';
        if (!valid_day) {
            continue;
        }
        const int day = std::stoi(day_text);
        if (day < 1 || day > days) {
            continue;
        }
        const int words = static_cast<int>(Clamped(entry.second.words));
        grid.values[static_cast<std::size_t>(offset + day - 1)] = words;
        if (words > grid.month_max) {
            grid.month_max = words;
        }
    }
    return grid;
}

inline std::vector<std::string> NavigableMonthsFor(const std::map<std::string, DayStats>& snapshot,
                                                   const std::string& current_month_key) {
    std::vector<std::string> months;
    for (const auto& entry : snapshot) {
        if (entry.first.size() < 7 || Clamped(entry.second.words) <= 0) {
            continue;
        }
        const std::string month_key = entry.first.substr(0, 7);
        int year = 0;
        int month = 0;
        if (!ParseMonthKey(month_key, year, month)) {
            continue;
        }
        months.push_back(month_key);
    }
    std::sort(months.begin(), months.end());
    months.erase(std::unique(months.begin(), months.end()), months.end());
    int year = 0;
    int month = 0;
    if (ParseMonthKey(current_month_key, year, month) &&
        std::find(months.begin(), months.end(), current_month_key) == months.end()) {
        months.push_back(current_month_key);
        std::sort(months.begin(), months.end());
    }
    return months;
}

inline std::string PrevNavigable(const std::map<std::string, DayStats>& snapshot,
                                 const std::string& current_month_key, const std::string& view) {
    const std::vector<std::string> months = NavigableMonthsFor(snapshot, current_month_key);
    std::string previous;
    for (const std::string& month_key : months) {
        if (month_key < view && month_key > previous) {
            previous = month_key;
        }
    }
    return previous;
}

inline std::string NextNavigable(const std::map<std::string, DayStats>& snapshot,
                                 const std::string& current_month_key, const std::string& view) {
    const std::vector<std::string> months = NavigableMonthsFor(snapshot, current_month_key);
    std::string next;
    for (const std::string& month_key : months) {
        if (month_key > view && (next.empty() || month_key < next)) {
            next = month_key;
        }
    }
    return next;
}

inline std::vector<std::string> ShareHighlightLines(const InsightsSummary& summary) {
    return {StreakTitle(summary.current_streak), WpmLabel(summary.wpm) + " wpm",
            std::to_string(summary.total_words) + " words",
            std::to_string(summary.total_apps) + " apps"};
}

class InsightsPage {
public:
    explicit InsightsPage(StatsStore& store) : store_(store) {}

    InsightsPage(const InsightsPage&) = delete;
    InsightsPage& operator=(const InsightsPage&) = delete;

    void Draw(float scale);

    bool IsShareOpen() const {
        return share_open_;
    }

    void OpenShare() {
        share_open_ = true;
    }

    void CloseShare() {
        share_open_ = false;
    }

    int Tab() const {
        return tab_;
    }

    void SetTab(int tab) {
        if (tab == 0 || tab == 1) {
            tab_ = tab;
        }
    }

    bool IsVoice() const {
        return tab_ == 1;
    }

    std::uint64_t LastGeneration() const {
        return last_generation_;
    }

    const InsightsSummary& Summary() const {
        return summary_;
    }

    const std::map<std::string, DayStats>& CachedSnapshot() const {
        return snapshot_;
    }

    std::string ViewMonth() const {
        return MonthKeyFor(view_year_, view_month_);
    }

    void SetViewMonth(int year, int month) {
        view_year_ = year;
        view_month_ = month;
        view_initialized_ = true;
    }

    bool CanPrevMonth() const {
        return !PrevNavigable(snapshot_, CurrentMonthKey(), ViewMonth()).empty();
    }

    bool CanNextMonth() const {
        return !NextNavigable(snapshot_, CurrentMonthKey(), ViewMonth()).empty();
    }

    bool GoPrevMonth() {
        const std::string target = PrevNavigable(snapshot_, CurrentMonthKey(), ViewMonth());
        if (target.empty()) {
            return false;
        }
        int year = 0;
        int month = 0;
        if (!ParseMonthKey(target, year, month)) {
            return false;
        }
        SetViewMonth(year, month);
        return true;
    }

    bool GoNextMonth() {
        const std::string target = NextNavigable(snapshot_, CurrentMonthKey(), ViewMonth());
        if (target.empty()) {
            return false;
        }
        int year = 0;
        int month = 0;
        if (!ParseMonthKey(target, year, month)) {
            return false;
        }
        SetViewMonth(year, month);
        return true;
    }

    bool SyncIfStale() {
        const std::uint64_t generation = store_.Generation();
        if (snapshot_valid_ && generation == last_generation_) {
            return false;
        }
        snapshot_ = store_.Snapshot();
        last_generation_ = generation;
        snapshot_valid_ = true;
        Recompute(TodayKeyNow());
        return true;
    }

    bool SyncFromSnapshot(const std::map<std::string, DayStats>& snapshot,
                          const std::string& today_key) {
        snapshot_ = snapshot;
        last_generation_ = store_.Generation();
        snapshot_valid_ = true;
        Recompute(today_key);
        return true;
    }

private:
    std::string CurrentMonthKey() const {
        if (current_month_key_.empty()) {
            return TodayKeyNow().substr(0, 7);
        }
        return current_month_key_;
    }

    void Recompute(const std::string& today_key) {
        summary_ = SummarizeSnapshot(snapshot_, today_key);
        current_month_key_ = today_key.size() >= 7 ? today_key.substr(0, 7) : std::string();
        if (!view_initialized_) {
            int year = 0;
            int month = 0;
            if (ParseMonthKey(current_month_key_, year, month)) {
                view_year_ = year;
                view_month_ = month;
            } else {
                view_year_ = 2026;
                view_month_ = 11;
            }
            view_initialized_ = true;
        }
    }

    void DrawHeader(float scale);
    void DrawTabs(float scale);
    void DrawRow1(float scale);
    void DrawCardA(float width, float scale);
    void DrawCardB(float width, float scale);
    void DrawCardC(float width, float scale);
    void DrawRow2(float scale);
    void DrawCardD(float width, float scale);
    void DrawCardE(float width, float scale);
    void DrawShareDialog(float scale);

    StatsStore& store_;
    widgets::Modal share_modal_;
    bool share_open_ = false;
    int tab_ = 0;
    bool snapshot_valid_ = false;
    std::uint64_t last_generation_ = 0;
    std::map<std::string, DayStats> snapshot_;
    InsightsSummary summary_;
    std::string current_month_key_;
    int view_year_ = 2026;
    int view_month_ = 11;
    bool view_initialized_ = false;
};

}
