#pragma once

#include <cstdint>
#include <vector>

std::vector<std::uint8_t> EncodeWav16Mono(const std::vector<float>& audio_pcm,
                                          std::uint32_t sample_rate);
