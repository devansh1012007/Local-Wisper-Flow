#include "core/AudioGain.h"

#include <algorithm>
#include <cmath>

namespace AudioGain {

namespace {

constexpr double kTargetRms = 0.1;
constexpr double kPeakLimit = 0.95;
constexpr double kMaxGain = 12.0;
constexpr double kMinSpeechRatio = 0.15;
constexpr double kNoiseFloorRms = 0.002;

}

std::vector<float> NormalizeLoudness(const std::vector<float>& pcm, bool enabled,
                                     double speech_ratio) {
    if (!enabled || pcm.empty()) {
        return pcm;
    }
    if (speech_ratio < kMinSpeechRatio) {
        return pcm;
    }

    double sum_squares = 0.0;
    float peak = 0.0f;
    for (const float sample : pcm) {
        sum_squares += static_cast<double>(sample) * static_cast<double>(sample);
        peak = std::max(peak, std::fabs(sample));
    }
    const double rms = std::sqrt(sum_squares / static_cast<double>(pcm.size()));
    if (rms < kNoiseFloorRms || rms >= kTargetRms) {
        return pcm;
    }

    double gain = kTargetRms / rms;
    if (gain > kMaxGain) {
        gain = kMaxGain;
    }

    std::vector<float> out;
    out.reserve(pcm.size());
    for (const float sample : pcm) {
        const double amplified = static_cast<double>(sample) * gain;
        out.push_back(static_cast<float>(std::clamp(amplified, -kPeakLimit, kPeakLimit)));
    }
    (void)peak;
    return out;
}

}
