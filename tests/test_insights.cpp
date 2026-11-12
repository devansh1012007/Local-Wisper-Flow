#include "core/InsightsModel.h"

#include <doctest/doctest.h>

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

TEST_CASE("Wpm clamps negative words and needs positive speech") {
    const auto negative = lwf::Wpm(-50, 60000);
    REQUIRE(negative.has_value());
    CHECK(negative.value() == doctest::Approx(0.0));
    CHECK_FALSE(lwf::Wpm(100, -5).has_value());
}

TEST_CASE("Wpm serves both Insights rates from one accumulator pair") {
    // Spoken divides by the mic-open time; written divides by the longer
    // end-to-end session time, so the written rate can never exceed the
    // spoken rate for the same words.
    const auto spoken = lwf::Wpm(150, 60000);
    const auto written = lwf::Wpm(150, 90000);
    REQUIRE(spoken.has_value());
    REQUIRE(written.has_value());
    CHECK(spoken.value() == doctest::Approx(150.0));
    CHECK(written.value() == doctest::Approx(100.0));
    CHECK(written.value() <= spoken.value());
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
    CHECK(lwf::CalendarLevel(24, 100) == 1);
    CHECK(lwf::CalendarLevel(150, 100) == 4);
    CHECK(lwf::CalendarLevel(-5, 100) == 0);
}
