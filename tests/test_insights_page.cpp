#include "ui/pages/InsightsPage.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

std::string TempDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_insightspage_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

std::int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

DayStats MakeDay(std::int64_t words, std::int64_t speaking_ms, std::int64_t corrected,
                 std::int64_t dict_fixes) {
    DayStats day;
    day.words = words;
    day.dictations = words > 0 ? 1 : 0;
    day.speaking_ms = speaking_ms;
    day.words_corrected = corrected;
    day.dictionary_fixes = dict_fixes;
    return day;
}

int ShareSumOf(const std::vector<lwf::ui::insights::CategoryRow>& rows) {
    int sum = 0;
    for (const auto& row : rows) {
        sum += row.share;
    }
    return sum;
}

}

TEST_CASE("insights page wpm floor at zero fifty-nine and sixty seconds") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-07"] = MakeDay(120, 0, 0, 0);
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    CHECK_FALSE(summary.wpm.has_value());
    CHECK(lwf::ui::insights::WpmLabel(summary.wpm) == lwf::ui::insights::kEmptyWpm);

    snapshot["2026-11-07"] = MakeDay(120, 59000, 0, 0);
    summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    CHECK_FALSE(summary.wpm.has_value());
    CHECK(lwf::ui::insights::WpmLabel(summary.wpm) == lwf::ui::insights::kEmptyWpm);

    snapshot["2026-11-07"] = MakeDay(120, 60000, 0, 0);
    summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    REQUIRE(summary.wpm.has_value());
    CHECK(summary.wpm.value() == doctest::Approx(120.0));
    CHECK(lwf::ui::insights::WpmLabel(summary.wpm) == "120");
    CHECK(lwf::ui::insights::GaugeFraction(summary.wpm) == doctest::Approx(0.6f));
}

TEST_CASE("insights page streak crosses midnight and month boundary") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-05"] = MakeDay(20, 60000, 0, 0);
    snapshot["2026-11-06"] = MakeDay(30, 60000, 0, 0);
    snapshot["2026-11-07"] = MakeDay(10, 60000, 0, 0);
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    CHECK(summary.current_streak == 3);
    CHECK(summary.longest_streak == 3);

    std::map<std::string, DayStats> month_boundary;
    month_boundary["2026-10-31"] = MakeDay(15, 60000, 0, 0);
    month_boundary["2026-11-01"] = MakeDay(15, 60000, 0, 0);
    summary = lwf::ui::insights::SummarizeSnapshot(month_boundary, "2026-11-01");
    CHECK(summary.current_streak == 2);
    CHECK(summary.longest_streak == 2);
}

TEST_CASE("insights page longest streak spans the longest run across a gap") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-01"] = MakeDay(10, 60000, 0, 0);
    snapshot["2026-11-02"] = MakeDay(10, 60000, 0, 0);
    snapshot["2026-11-03"] = MakeDay(10, 60000, 0, 0);
    snapshot["2026-11-06"] = MakeDay(10, 60000, 0, 0);
    snapshot["2026-11-07"] = MakeDay(10, 60000, 0, 0);
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    CHECK(summary.longest_streak == 3);
    CHECK(summary.current_streak == 2);
}

TEST_CASE("insights page calendar grid maps day words and month max") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-01"] = MakeDay(100, 60000, 0, 0);
    snapshot["2026-11-02"] = MakeDay(25, 60000, 0, 0);
    snapshot["2026-11-07"] = MakeDay(50, 60000, 0, 0);
    auto grid = lwf::ui::insights::MonthValuesFor(snapshot, 2026, 11);
    REQUIRE(grid.year == 2026);
    REQUIRE(grid.month == 11);
    CHECK(grid.month_max == 100);
    REQUIRE(grid.values.size() == 30);
    CHECK(grid.values[0] == 100);
    CHECK(grid.values[1] == 25);
    CHECK(grid.values[6] == 50);
    CHECK(lwf::CalendarLevel(grid.values[0], grid.month_max) == 4);
    CHECK(lwf::CalendarLevel(grid.values[1], grid.month_max) == 1);
    CHECK(lwf::CalendarLevel(grid.values[6], grid.month_max) == 2);

    std::map<std::string, DayStats> empty;
    auto empty_grid = lwf::ui::insights::MonthValuesFor(empty, 2026, 11);
    CHECK(empty_grid.month_max == 0);
    REQUIRE(empty_grid.values.size() == 30);
    for (int value : empty_grid.values) {
        CHECK(value == 0);
    }
}

TEST_CASE("insights page category shares sum to one hundred or zero") {
    std::map<std::string, DayStats> snapshot;
    DayStats day = MakeDay(100, 60000, 0, 0);
    day.by_category["AI prompts"] = 30;
    day.by_category["Emails"] = 30;
    day.by_category["Work messages"] = 40;
    snapshot["2026-11-07"] = day;
    auto rows = lwf::ui::insights::CategoryRowsFor(snapshot);
    REQUIRE(rows.size() == 6);
    CHECK(rows[0].key == "AI prompts");
    CHECK(rows[1].key == "Emails");
    CHECK(rows[2].key == "Work messages");
    CHECK(rows[3].key == "Personal");
    CHECK(rows[4].key == "Documents");
    CHECK(rows[5].key == "Other tasks");
    CHECK(ShareSumOf(rows) == 100);
    CHECK(rows[0].words == 30);
    CHECK(rows[0].fraction == doctest::Approx(0.3f));
    CHECK(rows[0].display_label == "AI PROMPTS");
    CHECK(lwf::ui::insights::PillLabel(rows[0].words, rows[0].display_label) == "30 AI PROMPTS");

    std::map<std::string, DayStats> empty;
    auto empty_rows = lwf::ui::insights::CategoryRowsFor(empty);
    REQUIRE(empty_rows.size() == 6);
    CHECK(ShareSumOf(empty_rows) == 0);
    for (const auto& row : empty_rows) {
        CHECK(row.words == 0);
        CHECK(row.fraction == doctest::Approx(0.0f));
    }
}

TEST_CASE("insights page unknown categories never break the share total") {
    std::map<std::string, DayStats> snapshot;
    DayStats day = MakeDay(20, 60000, 0, 0);
    day.by_category["AI prompts"] = 10;
    day.by_category["Custom"] = 10;
    snapshot["2026-11-07"] = day;
    auto rows = lwf::ui::insights::CategoryRowsFor(snapshot);
    REQUIRE(rows.size() == 6);
    CHECK(ShareSumOf(rows) == 100);
    CHECK(rows[0].share == 100);
    CHECK(rows[0].fraction == doctest::Approx(1.0f));
}

TEST_CASE("insights page distinct app count unions apps across days") {
    std::map<std::string, DayStats> snapshot;
    DayStats first = MakeDay(10, 60000, 0, 0);
    first.apps["notepad"] = 10;
    DayStats second = MakeDay(10, 60000, 0, 0);
    second.apps["notepad"] = 5;
    second.apps["chrome"] = 5;
    DayStats third = MakeDay(10, 60000, 0, 0);
    third.apps[""] = 10;
    snapshot["2026-11-06"] = first;
    snapshot["2026-11-07"] = second;
    snapshot["2026-11-08"] = third;
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-08");
    CHECK(summary.total_apps == 2);
}

TEST_CASE("insights page fixes total splits into corrected and dictionary rows") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-07"] = MakeDay(100, 60000, 7, 5);
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    CHECK(summary.words_corrected == 7);
    CHECK(summary.dictionary_fixes == 5);
    CHECK(summary.fixes_total == 12);
    CHECK(lwf::ui::insights::StreakTitle(1) == "1 day streak");
    CHECK(lwf::ui::insights::DesktopWordsLine(974) == "Desktop / 974 words");
}

TEST_CASE("insights page empty snapshot shows zero dash and empty calendar") {
    std::map<std::string, DayStats> snapshot;
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot, "2026-11-07");
    CHECK(summary.total_words == 0);
    CHECK_FALSE(summary.has_data);
    CHECK(lwf::ui::insights::WpmLabel(summary.wpm) == lwf::ui::insights::kEmptyWpm);
    CHECK(summary.total_apps == 0);
    CHECK(summary.current_streak == 0);
    CHECK(summary.longest_streak == 0);
    auto rows = lwf::ui::insights::CategoryRowsFor(snapshot);
    REQUIRE(rows.size() == 6);
    CHECK(ShareSumOf(rows) == 0);
    auto grid = lwf::ui::insights::MonthValuesFor(snapshot, 2026, 11);
    CHECK(grid.month_max == 0);
}

TEST_CASE("insights page generation counter drives refresh without polling") {
    const std::string dir = TempDir("generation");
    StatsStore store(dir + "/stats.json", true);
    lwf::ui::insights::InsightsPage page(store);
    REQUIRE(page.SyncIfStale());
    CHECK(page.Summary().total_words == 0);
    CHECK_FALSE(page.SyncIfStale());

    DictationRecord record;
    record.timestamp_local = NowSeconds();
    record.duration_ms = 60000;
    record.final_text = "hello world";
    record.process_name = "notepad";
    store.Record(record);
    REQUIRE(store.Generation() > 0);
    REQUIRE(page.SyncIfStale());
    CHECK(page.Summary().total_words == 2);
    CHECK(page.LastGeneration() == store.Generation());
    CHECK_FALSE(page.SyncIfStale());
}

TEST_CASE("insights page month navigator visits data months plus current") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-09-10"] = MakeDay(10, 60000, 0, 0);
    snapshot["2026-11-02"] = MakeDay(10, 60000, 0, 0);
    auto months = lwf::ui::insights::NavigableMonthsFor(snapshot, "2026-11");
    REQUIRE(months.size() == 2);
    CHECK(months[0] == "2026-09");
    CHECK(months[1] == "2026-11");
    CHECK(lwf::ui::insights::PrevNavigable(snapshot, "2026-11", "2026-11") == "2026-09");
    CHECK(lwf::ui::insights::PrevNavigable(snapshot, "2026-11", "2026-09").empty());
    CHECK(lwf::ui::insights::NextNavigable(snapshot, "2026-11", "2026-09") == "2026-11");
    CHECK(lwf::ui::insights::NextNavigable(snapshot, "2026-11", "2026-11").empty());

    const std::string dir = TempDir("navigator");
    StatsStore store(dir + "/stats.json", true);
    lwf::ui::insights::InsightsPage page(store);
    REQUIRE(page.SyncFromSnapshot(snapshot, "2026-11-02"));
    CHECK(page.ViewMonth() == "2026-11");
    CHECK(page.GoPrevMonth());
    CHECK(page.ViewMonth() == "2026-09");
    CHECK_FALSE(page.GoPrevMonth());
    CHECK(page.GoNextMonth());
    CHECK(page.ViewMonth() == "2026-11");
    CHECK_FALSE(page.GoNextMonth());
    CHECK(page.CanPrevMonth());
}

TEST_CASE("insights page calendar helpers cover leap months and sunday start") {
    CHECK(lwf::ui::insights::DaysInMonth(2026, 2) == 28);
    CHECK(lwf::ui::insights::DaysInMonth(2024, 2) == 29);
    CHECK(lwf::ui::insights::DaysInMonth(2026, 11) == 30);
    CHECK(lwf::ui::insights::WeekdayOfFirst(2026, 11) == 0);
    CHECK(lwf::ui::insights::WeekdayOfFirst(2026, 9) == 2);
    CHECK(lwf::ui::insights::MonthLabelFor("2026-11") == "November 2026");
    CHECK(lwf::ui::insights::MonthLabelFor("not-a-month").empty());
}

TEST_CASE("insights page copy constants pin card strings") {
    CHECK(std::string(lwf::ui::insights::kPageTitle) == "Insights");
    CHECK(std::string(lwf::ui::insights::kTabUsage) == "Your usage");
    CHECK(std::string(lwf::ui::insights::kWpmTooltip) ==
          "Your average speaking speed while dictating.");
    CHECK(std::string(lwf::ui::insights::kGaugeFootnote) ==
          "Ranking against other users needs an account, which this app does not have.");
    CHECK(std::string(lwf::ui::insights::kMobileTooltip) ==
          "Mobile is not available in this app.");
    CHECK(std::string(lwf::ui::insights::kDesktopEmpty) ==
          "No dictations yet. Hold Ctrl + Win to start.");
    CHECK(lwf::ui::insights::kWpmGaugeCap == doctest::Approx(200.0));
}

TEST_CASE("insights page source scan keeps spec copy and bans") {
    std::ifstream header_stream("src/ui/pages/InsightsPage.h");
    std::ifstream impl_stream("src/ui/pages/InsightsPage.cpp");
    REQUIRE(header_stream.is_open());
    REQUIRE(impl_stream.is_open());
    const std::string header((std::istreambuf_iterator<char>(header_stream)),
                             std::istreambuf_iterator<char>());
    const std::string impl((std::istreambuf_iterator<char>(impl_stream)),
                           std::istreambuf_iterator<char>());
    const std::string both = header + "\n" + impl;
    CHECK_FALSE(header.empty());
    CHECK_FALSE(impl.empty());
    CHECK(both.find("Insights") != std::string::npos);
    CHECK(both.find("Your usage") != std::string::npos);
    // T10d: voice tab + share dialog removed honestly (no stub remains).
    CHECK(both.find("Your voice") == std::string::npos);
    CHECK(both.find("Not available yet.") == std::string::npos);
    CHECK(both.find("kTabVoice") == std::string::npos);
    CHECK(both.find("kVoiceEmpty") == std::string::npos);
    CHECK(both.find("IsVoice") == std::string::npos);
    CHECK(both.find("WORDS PER MINUTE") != std::string::npos);
    CHECK(both.find("Your average speaking speed while dictating.") != std::string::npos);
    CHECK(both.find("Average") != std::string::npos);
    CHECK(both.find("Ranking against other users needs an account, which this app does not have.") !=
          std::string::npos);
    CHECK(both.find("FIXES MADE BY FLOW") != std::string::npos);
    CHECK(both.find("words corrected") != std::string::npos);
    CHECK(both.find("dictionary fixes") != std::string::npos);
    CHECK(both.find("TOTAL WORDS DICTATED") != std::string::npos);
    CHECK(both.find("Desktop") != std::string::npos);
    CHECK(both.find("Download on mobile") != std::string::npos);
    CHECK(both.find("Mobile is not available in this app.") != std::string::npos);
    CHECK(both.find("Desktop usage") != std::string::npos);
    CHECK(both.find("TOTAL APPS USED | ") != std::string::npos);
    CHECK(both.find("AI PROMPTS") != std::string::npos);
    CHECK(both.find("EMAILS") != std::string::npos);
    CHECK(both.find("WORK MESSAGES") != std::string::npos);
    CHECK(both.find("PERSONAL MESSAGES") != std::string::npos);
    CHECK(both.find("DOCUMENTS") != std::string::npos);
    CHECK(both.find("OTHER TASKS") != std::string::npos);
    CHECK(both.find("LONGEST STREAK | ") != std::string::npos);
    CHECK(both.find("No dictations yet. Hold Ctrl + Win to start.") != std::string::npos);
    // T10d: share stubs stay gone (honest removal, no replacement copy).
    CHECK(both.find("Flow Highlights") == std::string::npos);
    CHECK(both.find("Flow Streak") == std::string::npos);
    CHECK(both.find("Copy image") == std::string::npos);
    CHECK(both.find("Download image") == std::string::npos);
    CHECK(both.find("Coming soon") == std::string::npos);
    CHECK(both.find("kComingSoon") == std::string::npos);
    CHECK(both.find("SHARE") == std::string::npos);
    CHECK(both.find("ShareHighlightLines") == std::string::npos);
    CHECK(both.find("DrawShareDialog") == std::string::npos);
    CHECK(both.find("OpenShare") == std::string::npos);
    CHECK(both.find("share_open_") == std::string::npos);
    CHECK(both.find("Generation()") != std::string::npos);
    // T10d: single Usage view carries no tab row (tab state removed).
    CHECK(both.find("TabRow") == std::string::npos);
    CHECK(both.find("RadialGauge") != std::string::npos);
    CHECK(both.find("ProgressPill") != std::string::npos);
    CHECK(both.find("ActivityGrid") != std::string::npos);
    CHECK(both.find("OutlinePill") != std::string::npos);
    CHECK(both.find("Top 0.1%") == std::string::npos);
    CHECK(both.find("SetWindowFontScale") == std::string::npos);
    CHECK(both.find("windows.h") == std::string::npos);
    CHECK(both.find("TODO") == std::string::npos);
    CHECK(both.find("FIXME") == std::string::npos);
    CHECK(both.find("XXX") == std::string::npos);
    CHECK(header.find("imgui.h") == std::string::npos);
    // T10d: live palette threaded explicitly (T10a/b pattern); no Paper read
    // remains and card chrome is hairlines with zero BeginCard calls.
    CHECK(header.find("Draw(float scale, const lwf::ui::Palette") != std::string::npos);
    CHECK(header.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("Color(palette.bg)") != std::string::npos);
    CHECK(impl.find("Pack(palette.border)") != std::string::npos);
    CHECK(impl.find("Color(palette.muted)") != std::string::npos);
    CHECK(impl.find("Pack(palette.text)") != std::string::npos);
    CHECK(impl.find("Pack(palette.accent)") != std::string::npos);
    CHECK(impl.find("BeginCard(") == std::string::npos);
    CHECK(impl.find("BeginChild") != std::string::npos);
}
