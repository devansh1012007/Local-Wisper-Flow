#include "core/AudioGain.h"

#include <doctest/doctest.h>
#include <cmath>
#include <vector>

namespace {

double RmsOf(const std::vector<float>& pcm) {
    double sum = 0.0;
    for (const float sample : pcm) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return std::sqrt(sum / static_cast<double>(pcm.size()));
}

std::vector<float> QuietTone(int samples) {
    std::vector<float> pcm;
    pcm.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / 16000.0;
        const double tone = 0.008 * std::sin(2.0 * 3.14159265358979 * 220.0 * t);
        const double noise = 0.002 * (std::sin(37.0 * t) * 0.5 + std::sin(91.3 * t) * 0.5);
        pcm.push_back(static_cast<float>(tone + noise));
    }
    return pcm;
}

}

TEST_CASE("quiet audio is boosted toward the target rms") {
    const std::vector<float> quiet = QuietTone(16000);
    const double before = RmsOf(quiet);
    CHECK(before < 0.05);

    const std::vector<float> boosted = AudioGain::NormalizeLoudness(quiet, true, 0.8);
    const double after = RmsOf(boosted);
    CHECK(after > before * 4.0);
    CHECK(after < 0.2);
}

TEST_CASE("boosted audio never clips") {
    const std::vector<float> quiet = QuietTone(8000);
    const std::vector<float> boosted = AudioGain::NormalizeLoudness(quiet, true, 0.8);
    for (const float sample : boosted) {
        CHECK(std::fabs(sample) <= 0.9500001f);
    }
}

TEST_CASE("loud audio passes through unchanged") {
    std::vector<float> loud;
    for (int i = 0; i < 4000; ++i) {
        loud.push_back(0.6f * std::sin(0.05f * static_cast<float>(i)));
    }
    const std::vector<float> result = AudioGain::NormalizeLoudness(loud, true, 0.8);
    CHECK(result == loud);
}

TEST_CASE("low speech ratio and disabled flag skip the boost") {
    const std::vector<float> quiet = QuietTone(4000);
    CHECK(AudioGain::NormalizeLoudness(quiet, true, 0.05) == quiet);
    CHECK(AudioGain::NormalizeLoudness(quiet, false, 0.8) == quiet);
    CHECK(AudioGain::NormalizeLoudness({}, true, 0.8).empty());

    std::vector<float> near_silence(4000, 0.0001f);
    CHECK(AudioGain::NormalizeLoudness(near_silence, true, 0.8) == near_silence);
}
