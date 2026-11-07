#include "core/InsightsModel.h"

#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace {

lwf::DayRecord MakeRecord(const std::string& day, std::int64_t words) {
    lwf::DayRecord record;
    record.day = day;
    record.words = words;
    record.speaking_ms = words > 0 ? 60000 : 0;
    record.app = "notepad";
    record.category = "notes";
    return record;
}

int ShareSum(const std::vector<std::pair<std::string, int>>& shares) {
    int sum = 0;
    for (const auto& share : shares) {
        sum += share.second;
    }
    return sum;
}

int ShareOf(const std::vector<std::pair<std::string, int>>& shares, const std::string& category) {
    for (const auto& share : shares) {
        if (share.first == category) {
            return share.second;
        }
    }
    return -1;
}

}

TEST_CASE("DayKey zero pads year month and day") {
    CHECK(lwf::DayKey(2026, 11, 7) == "2026-11-07");
    CHECK(lwf::DayKey(2026, 1, 5) == "2026-01-05");
    CHECK(lwf::DayKey(1999, 12, 31) == "1999-12-31");
}

TEST_CASE("Wpm is empty below sixty seconds of speech") {
    CHECK_FALSE(lwf::Wpm(100, 0).has_value());
    CHECK_FALSE(lwf::Wpm(100, 30000).has_value());
    CHECK_FALSE(lwf::Wpm(100, 59000).has_value());
}

TEST_CASE("Wpm computes words per minute at and above the floor") {
    const auto at_floor = lwf::Wpm(120, 60000);
    REQUIRE(at_floor.has_value());
    CHECK(at_floor.value() == doctest::Approx(120.0));

    const auto two_minutes = lwf::Wpm(150, 120000);
    REQUIRE(two_minutes.has_value());
    CHECK(two_minutes.value() == doctest::Approx(75.0));

    const auto no_words = lwf::Wpm(0, 90000);
    REQUIRE(no_words.has_value());
    CHECK(no_words.value() == doctest::Approx(0.0));
}

TEST_CASE("FixesMadeByFlow sums positive fix counts") {
    lwf::DayRecord a = MakeRecord("2026-11-06", 50);
    a.fixes = 3;
    lwf::DayRecord b = MakeRecord("2026-11-07", 40);
    b.fixes = 7;
    lwf::DayRecord c = MakeRecord("2026-11-07", 10);
    c.fixes = -2;
    const std::vector<lwf::DayRecord> records{a, b, c};
    CHECK(lwf::FixesMadeByFlow(records) == 10);
    CHECK(lwf::FixesMadeByFlow(std::vector<lwf::DayRecord>{}) == 0);
}

TEST_CASE("CategoryShares sums to exactly 100 with words") {
    lwf::DayRecord a = MakeRecord("2026-11-07", 30);
    a.category = "notes";
    lwf::DayRecord b = MakeRecord("2026-11-07", 30);
    b.category = "code";
    lwf::DayRecord c = MakeRecord("2026-11-06", 40);
    c.category = "email";
    const std::vector<lwf::DayRecord> records{a, b, c};

    const auto shares = lwf::CategoryShares(records);
    REQUIRE(shares.size() == 3);
    CHECK(ShareSum(shares) == 100);
    CHECK(ShareOf(shares, "email") == 40);
    CHECK(ShareOf(shares, "notes") + ShareOf(shares, "code") == 60);
}

TEST_CASE("CategoryShares rounds thirds so the shares still sum to 100") {
    lwf::DayRecord a = MakeRecord("2026-11-07", 1);
    a.category = "a";
    lwf::DayRecord b = MakeRecord("2026-11-07", 1);
    b.category = "b";
    lwf::DayRecord c = MakeRecord("2026-11-07", 1);
    c.category = "c";
    const std::vector<lwf::DayRecord> records{a, b, c};

    const auto shares = lwf::CategoryShares(records);
    REQUIRE(shares.size() == 3);
    CHECK(ShareSum(shares) == 100);
}

TEST_CASE("CategoryShares is empty with no words") {
    CHECK(lwf::CategoryShares(std::vector<lwf::DayRecord>{}).empty());
    const std::vector<lwf::DayRecord> zero{MakeRecord("2026-11-07", 0)};
    CHECK(lwf::CategoryShares(zero).empty());
}

TEST_CASE("TotalAppsUsed counts distinct non-empty apps") {
    lwf::DayRecord a = MakeRecord("2026-11-07", 10);
    a.app = "notepad";
    lwf::DayRecord b = MakeRecord("2026-11-07", 10);
    b.app = "notepad";
    lwf::DayRecord c = MakeRecord("2026-11-07", 10);
    c.app = "chrome";
    lwf::DayRecord d = MakeRecord("2026-11-07", 10);
    d.app = "";
    const std::vector<lwf::DayRecord> records{a, b, c, d};
    CHECK(lwf::TotalAppsUsed(records) == 2);
    CHECK(lwf::TotalAppsUsed(std::vector<lwf::DayRecord>{}) == 0);
}

TEST_CASE("CurrentStreak needs words today and counts back through midnight") {
    const std::vector<lwf::DayRecord> records{
        MakeRecord("2026-11-05", 20),
        MakeRecord("2026-11-06", 30),
        MakeRecord("2026-11-07", 10),
    };
    CHECK(lwf::CurrentStreak(records, "2026-11-07") == 3);
    CHECK(lwf::CurrentStreak(records, "2026-11-08") == 0);
}

TEST_CASE("CurrentStreak is zero when today has no words") {
    const std::vector<lwf::DayRecord> records{
        MakeRecord("2026-11-06", 30),
        MakeRecord("2026-11-07", 0),
    };
    CHECK(lwf::CurrentStreak(records, "2026-11-07") == 0);
}

TEST_CASE("CurrentStreak crosses month boundaries") {
    const std::vector<lwf::DayRecord> records{
        MakeRecord("2026-10-31", 15),
        MakeRecord("2026-11-01", 15),
    };
    CHECK(lwf::CurrentStreak(records, "2026-11-01") == 2);
}

TEST_CASE("LongestStreak spans the longest run across gaps") {
    const std::vector<lwf::DayRecord> records{
        MakeRecord("2026-11-01", 10),
        MakeRecord("2026-11-02", 10),
        MakeRecord("2026-11-03", 10),
        MakeRecord("2026-11-06", 10),
        MakeRecord("2026-11-07", 10),
    };
    CHECK(lwf::LongestStreak(records) == 3);
    CHECK(lwf::LongestStreak(std::vector<lwf::DayRecord>{}) == 0);
}

TEST_CASE("CalendarLevel is zero when the month max is zero") {
    CHECK(lwf::CalendarLevel(0, 0) == 0);
    CHECK(lwf::CalendarLevel(10, 0) == 0);
    CHECK(lwf::CalendarLevel(0, 100) == 0);
}

TEST_CASE("CalendarLevel maps the quartiles of the month max") {
    CHECK(lwf::CalendarLevel(25, 100) == 1);
    CHECK(lwf::CalendarLevel(26, 100) == 1);
    CHECK(lwf::CalendarLevel(50, 100) == 2);
    CHECK(lwf::CalendarLevel(75, 100) == 3);
    CHECK(lwf::CalendarLevel(100, 100) == 4);
    CHECK(lwf::CalendarLevel(1, 100) == 1);
    CHECK(lwf::CalendarLevel(150, 100) == 4);
}
