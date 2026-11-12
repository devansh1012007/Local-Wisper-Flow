#include "ui/pages/InsightsPage.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>

namespace {

std::string TempDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_insightspage_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

DayStats MakeDay(std::int64_t words, std::int64_t speaking_ms, std::int64_t session_ms) {
    DayStats day;
    day.words = words;
    day.dictations = words > 0 ? 1 : 0;
    day.speaking_ms = speaking_ms;
    day.session_ms = session_ms;
    return day;
}

}

TEST_CASE("insights page summarizes spoken and written wpm from one snapshot") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-07"] = MakeDay(120, 60000, 90000);
    const auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    CHECK(summary.total_words == 120);
    CHECK(summary.total_speaking_ms == 60000);
    CHECK(summary.total_session_ms == 90000);
    REQUIRE(summary.spoken_wpm.has_value());
    CHECK(summary.spoken_wpm.value() == doctest::Approx(120.0));
    REQUIRE(summary.written_wpm.has_value());
    CHECK(summary.written_wpm.value() == doctest::Approx(80.0));
    CHECK(lwf::ui::insights::WpmLabel(summary.spoken_wpm) == "120");
    CHECK(lwf::ui::insights::WpmLabel(summary.written_wpm) == "80");
    CHECK(summary.has_data);
}

TEST_CASE("insights page accumulates both durations across days") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-06"] = MakeDay(60, 30000, 45000);
    snapshot["2026-11-07"] = MakeDay(60, 30000, 45000);
    const auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    CHECK(summary.total_words == 120);
    CHECK(summary.total_speaking_ms == 60000);
    CHECK(summary.total_session_ms == 90000);
    REQUIRE(summary.spoken_wpm.has_value());
    CHECK(summary.spoken_wpm.value() == doctest::Approx(120.0));
    REQUIRE(summary.written_wpm.has_value());
    CHECK(summary.written_wpm.value() == doctest::Approx(80.0));
}

TEST_CASE("insights page written wpm falls back to speaking time for legacy days") {
    // Days recorded before session_ms existed carry 0; the written rate must
    // reuse the speaking time instead of going empty.
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-07"] = MakeDay(120, 60000, 0);
    const auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    REQUIRE(summary.spoken_wpm.has_value());
    REQUIRE(summary.written_wpm.has_value());
    CHECK(summary.written_wpm.value() == doctest::Approx(summary.spoken_wpm.value()));
    CHECK(summary.written_wpm.value() == doctest::Approx(120.0));
}

TEST_CASE("insights page mixed legacy and tracked days still prefer session time") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-06"] = MakeDay(60, 30000, 0);
    snapshot["2026-11-07"] = MakeDay(60, 30000, 60000);
    const auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    CHECK(summary.total_session_ms == 60000);
    REQUIRE(summary.written_wpm.has_value());
    CHECK(summary.written_wpm.value() == doctest::Approx(120.0));
}

TEST_CASE("insights page wpm floor at fifty-nine and sixty seconds") {
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-07"] = MakeDay(120, 0, 0);
    auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    CHECK_FALSE(summary.spoken_wpm.has_value());
    CHECK_FALSE(summary.written_wpm.has_value());
    CHECK(lwf::ui::insights::WpmLabel(summary.spoken_wpm) == lwf::ui::insights::kEmptyWpm);
    CHECK(lwf::ui::insights::WpmLabel(summary.written_wpm) == lwf::ui::insights::kEmptyWpm);
    CHECK(summary.has_data);

    snapshot["2026-11-07"] = MakeDay(120, 59000, 59000);
    summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    CHECK_FALSE(summary.spoken_wpm.has_value());
    CHECK_FALSE(summary.written_wpm.has_value());

    snapshot["2026-11-07"] = MakeDay(120, 60000, 60000);
    summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    REQUIRE(summary.spoken_wpm.has_value());
    REQUIRE(summary.written_wpm.has_value());
    CHECK(summary.spoken_wpm.value() == doctest::Approx(120.0));
    CHECK(summary.written_wpm.value() == doctest::Approx(120.0));
}

TEST_CASE("insights page empty snapshot shows dashes and no data") {
    std::map<std::string, DayStats> snapshot;
    const auto summary = lwf::ui::insights::SummarizeSnapshot(snapshot);
    CHECK(summary.total_words == 0);
    CHECK_FALSE(summary.has_data);
    CHECK_FALSE(summary.spoken_wpm.has_value());
    CHECK_FALSE(summary.written_wpm.has_value());
    CHECK(lwf::ui::insights::WpmLabel(summary.spoken_wpm) == lwf::ui::insights::kEmptyWpm);
    CHECK(lwf::ui::insights::WpmLabel(summary.written_wpm) == lwf::ui::insights::kEmptyWpm);
}

TEST_CASE("insights page syncs from an injected snapshot") {
    const std::string dir = TempDir("syncsnapshot");
    StatsStore store(dir + "/stats.json", true);
    lwf::ui::insights::InsightsPage page(store);
    std::map<std::string, DayStats> snapshot;
    snapshot["2026-11-07"] = MakeDay(100, 60000, 75000);
    REQUIRE(page.SyncFromSnapshot(snapshot));
    CHECK(page.Summary().total_words == 100);
    REQUIRE(page.Summary().written_wpm.has_value());
    CHECK(page.Summary().written_wpm.value() == doctest::Approx(80.0));
    CHECK(page.CachedSnapshot().size() == 1);
    CHECK(page.LastGeneration() == store.Generation());
    std::filesystem::remove_all(dir);
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
    record.session_ms = 90000;
    record.final_text = "hello world";
    record.process_name = "notepad";
    store.Record(record);
    REQUIRE(store.Generation() > 0);
    REQUIRE(page.SyncIfStale());
    CHECK(page.Summary().total_words == 2);
    CHECK(page.LastGeneration() == store.Generation());
    REQUIRE(page.Summary().spoken_wpm.has_value());
    CHECK(page.Summary().spoken_wpm.value() == doctest::Approx(2.0));
    REQUIRE(page.Summary().written_wpm.has_value());
    CHECK(page.Summary().written_wpm.value() == doctest::Approx(doctest::Approx(4.0 / 3.0).epsilon(0.001)));
    CHECK_FALSE(page.SyncIfStale());
    std::filesystem::remove_all(dir);
}

TEST_CASE("insights page copy constants pin card strings") {
    CHECK(std::string(lwf::ui::insights::kPageTitle) == "Insights");
    CHECK(std::string(lwf::ui::insights::kSpokenCaption) == "WORDS SPOKEN PER MINUTE");
    CHECK(std::string(lwf::ui::insights::kWrittenCaption) == "WORDS WRITTEN PER MINUTE");
    CHECK(std::string(lwf::ui::insights::kSpokenTooltip) ==
          "Your average speaking speed while dictating.");
    CHECK(std::string(lwf::ui::insights::kWrittenTooltip) ==
          "How fast your dictated words reach the page, transcription time included.");
    CHECK(std::string(lwf::ui::insights::kEmptyWpm) == "—");
    CHECK(std::string(lwf::ui::insights::kEmptyHint) ==
          "No dictations yet. Hold Ctrl + Win to start.");
    CHECK(std::string(lwf::ui::insights::kFloorHint) ==
          "Averages appear after your first minute of dictation.");
}

TEST_CASE("insights page source scan keeps the two-card spec and bans the rest") {
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
    // The two cards the page is now spec'd to show.
    CHECK(both.find("Insights") != std::string::npos);
    CHECK(both.find("WORDS SPOKEN PER MINUTE") != std::string::npos);
    CHECK(both.find("WORDS WRITTEN PER MINUTE") != std::string::npos);
    CHECK(both.find("Your average speaking speed while dictating.") != std::string::npos);
    CHECK(both.find("How fast your dictated words reach the page, transcription time "
                    "included.") != std::string::npos);
    CHECK(both.find("No dictations yet. Hold Ctrl + Win to start.") != std::string::npos);
    CHECK(both.find("Averages appear after your first minute of dictation.") != std::string::npos);
    CHECK(both.find("Generation()") != std::string::npos);
    CHECK(both.find("session_ms") != std::string::npos);
    // The removed cards stay gone (honest removal, no stubs).
    CHECK(both.find("kTabUsage") == std::string::npos);
    CHECK(both.find("Your usage") == std::string::npos);
    CHECK(both.find("FIXES MADE BY FLOW") == std::string::npos);
    CHECK(both.find("words corrected") == std::string::npos);
    CHECK(both.find("dictionary fixes") == std::string::npos);
    CHECK(both.find("TOTAL WORDS DICTATED") == std::string::npos);
    CHECK(both.find("Desktop usage") == std::string::npos);
    CHECK(both.find("TOTAL APPS USED") == std::string::npos);
    CHECK(both.find("AI PROMPTS") == std::string::npos);
    CHECK(both.find("LONGEST STREAK") == std::string::npos);
    CHECK(both.find("day streak") == std::string::npos);
    CHECK(both.find("Download on mobile") == std::string::npos);
    CHECK(both.find("Mobile is not available in this app.") == std::string::npos);
    CHECK(both.find("Ranking against other users") == std::string::npos);
    CHECK(both.find("RadialGauge") == std::string::npos);
    CHECK(both.find("ProgressPill") == std::string::npos);
    CHECK(both.find("ActivityGrid") == std::string::npos);
    CHECK(both.find("OutlinePill") == std::string::npos);
    CHECK(both.find("HeatColor") == std::string::npos);
    CHECK(both.find("GaugeFraction") == std::string::npos);
    CHECK(both.find("kWpmGaugeCap") == std::string::npos);
    CHECK(both.find("MonthValuesFor") == std::string::npos);
    CHECK(both.find("CategoryRowsFor") == std::string::npos);
    CHECK(both.find("GoPrevMonth") == std::string::npos);
    CHECK(both.find("CurrentStreak") == std::string::npos);
    CHECK(both.find("LongestStreak") == std::string::npos);
    CHECK(both.find("TabRow") == std::string::npos);
    CHECK(both.find("BeginCard(") == std::string::npos);
    CHECK(both.find("Coming soon") == std::string::npos);
    CHECK(both.find("Flow Highlights") == std::string::npos);
    // Mechanism bans shared with the other studio pages.
    CHECK(both.find("Top 0.1%") == std::string::npos);
    CHECK(both.find("SetWindowFontScale") == std::string::npos);
    CHECK(both.find("windows.h") == std::string::npos);
    CHECK(both.find("TODO") == std::string::npos);
    CHECK(both.find("FIXME") == std::string::npos);
    CHECK(both.find("XXX") == std::string::npos);
    CHECK(header.find("imgui.h") == std::string::npos);
    // Live palette threaded explicitly (T10d); no Paper read remains.
    CHECK(header.find("Draw(float scale, const lwf::ui::Palette") != std::string::npos);
    CHECK(header.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("kLightPalette") == std::string::npos);
    CHECK(impl.find("Color(palette.bg)") != std::string::npos);
    CHECK(impl.find("Color(palette.muted)") != std::string::npos);
    CHECK(impl.find("Pack(palette.text)") != std::string::npos);
    CHECK(impl.find("BeginChild") != std::string::npos);
}
