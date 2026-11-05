#pragma once

#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <mutex>
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

struct DeviceState {
    std::mutex mutex;
    std::condition_variable done_cv;
    std::vector<float> active;
    std::size_t position = 0;
};

std::vector<float> Generate(int sample_rate, double frequency_hz, int duration_ms,
                            int fade_ms, double volume);

ToneSpec StartTone();
ToneSpec StopTone();
ToneSpec ErrorTone();

}
