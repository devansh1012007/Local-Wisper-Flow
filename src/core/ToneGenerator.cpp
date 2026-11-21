#include "core/ToneGenerator.h"

#include <algorithm>
#include <cmath>

namespace ToneGenerator {

std::vector<float> Generate(int sample_rate, double frequency_hz, int duration_ms,
                            int fade_ms, double volume) {
    if (sample_rate <= 0 || duration_ms <= 0 || volume <= 0.0) {
        return {};
    }
    const std::size_t total = static_cast<std::size_t>(sample_rate) *
                              static_cast<std::size_t>(duration_ms) / 1000;
    const std::size_t fade = std::min(total, static_cast<std::size_t>(sample_rate) *
                                                   static_cast<std::size_t>(fade_ms) / 1000);

    std::vector<float> samples;
    samples.reserve(total);
    constexpr double kTwoPi = 6.28318530717958647692;
    for (std::size_t i = 0; i < total; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sample_rate);
        double amplitude = 1.0;
        if (fade > 0 && i < fade) {
            amplitude = static_cast<double>(i) / static_cast<double>(fade);
        } else if (fade > 0 && i >= total - fade) {
            amplitude = static_cast<double>(total - i) / static_cast<double>(fade);
        }
        const double value = std::sin(kTwoPi * frequency_hz * t) * volume * amplitude;
        samples.push_back(static_cast<float>(std::clamp(value, -volume, volume)));
    }
    return samples;
}

ToneSpec StartTone() {
    return {880.0, 60, 1.0};
}

ToneSpec StopTone() {
    return {660.0, 60, 1.0};
}

ToneSpec ErrorTone() {
    return {220.0, 150, 1.0};
}

ToneSpec PreviewTone() {
    // 523.25 Hz is middle C: a soft "ding" that cannot be confused with the
    // start (880) / stop (660) / error (220) blips, and 160 ms is long enough
    // to judge loudness at low amplitudes (60 ms was easy to miss entirely).
    return {523.25, 160, 1.0};
}

}
