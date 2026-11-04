#pragma once

#include <cstddef>
#include <vector>

namespace ToneGenerator {

struct ToneSpec {
    double frequency_hz;
    int duration_ms;
    double volume;
};

struct GeneratedTone {
    std::vector<float> samples;
    int sample_rate = 16000;
};

std::vector<float> Generate(int sample_rate, double frequency_hz, int duration_ms,
                            int fade_ms, double volume);

ToneSpec StartTone();
ToneSpec StopTone();
ToneSpec ErrorTone();

}
