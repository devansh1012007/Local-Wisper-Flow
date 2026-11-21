#include "core/ToneGenerator.h"

#include <doctest/doctest.h>
#include <cmath>
#include <string>
#include <vector>

namespace {

double PeakOf(const std::vector<float>& samples) {
    double peak = 0.0;
    for (const float sample : samples) {
        peak = std::max(peak, std::fabs(static_cast<double>(sample)));
    }
    return peak;
}

}

TEST_CASE("tone length matches duration and sample rate") {
    const auto samples = ToneGenerator::Generate(16000, 880.0, 60, 10, 0.3);
    CHECK(samples.size() == static_cast<std::size_t>(16000) * 60 / 1000);

    const auto long_tone = ToneGenerator::Generate(44100, 440.0, 1000, 10, 0.5);
    CHECK(long_tone.size() == 44100);

    CHECK(ToneGenerator::Generate(16000, 440.0, 0, 10, 0.5).empty());
    CHECK(ToneGenerator::Generate(16000, 440.0, 50, 10, 0.0).empty());
    CHECK(ToneGenerator::Generate(0, 440.0, 50, 10, 0.5).empty());
}

TEST_CASE("tones have fades at both ends and bounded amplitude") {
    const auto samples = ToneGenerator::Generate(16000, 660.0, 200, 10, 0.5);
    REQUIRE(samples.size() == 3200);
    CHECK(std::fabs(samples.front()) < 0.05);
    CHECK(std::fabs(samples.back()) < 0.05);
    double mid_peak = 0.0;
    for (std::size_t i = 800; i < 2400; ++i) {
        mid_peak = std::max(mid_peak, std::fabs(static_cast<double>(samples[i])));
    }
    CHECK(mid_peak > 0.1);
    CHECK(PeakOf(samples) <= 0.5000001);
}

TEST_CASE("fade longer than the tone collapses to zero length fades") {
    const auto samples = ToneGenerator::Generate(16000, 440.0, 5, 50, 0.8);
    CHECK(samples.size() == 80);
    CHECK(PeakOf(samples) <= 0.8000001);
}

TEST_CASE("presets match the spec") {
    const ToneGenerator::ToneSpec start = ToneGenerator::StartTone();
    CHECK(start.frequency_hz == doctest::Approx(880.0));
    CHECK(start.duration_ms == 60);
    const ToneGenerator::ToneSpec stop = ToneGenerator::StopTone();
    CHECK(stop.frequency_hz == doctest::Approx(660.0));
    CHECK(stop.duration_ms == 60);
    const ToneGenerator::ToneSpec error = ToneGenerator::ErrorTone();
    CHECK(error.frequency_hz == doctest::Approx(220.0));
    CHECK(error.duration_ms == 150);
}
