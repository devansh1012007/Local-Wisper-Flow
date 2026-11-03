#pragma once

#include <vector>

namespace AudioGain {

std::vector<float> NormalizeLoudness(const std::vector<float>& pcm, bool enabled,
                                     double speech_ratio);

}
