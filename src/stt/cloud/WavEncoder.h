#pragma once

#include <cstdint>
#include <vector>

// Encodes mono float PCM [-1, 1] as a 16-bit PCM RIFF/WAVE byte stream
// ("RIFF" magic first), clamping out-of-range samples.
std::vector<std::uint8_t> EncodeWav16Mono(const std::vector<float>& pcm, std::uint32_t sample_rate);
