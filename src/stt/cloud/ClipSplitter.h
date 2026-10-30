#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct AudioClip {
    std::size_t begin = 0;
    std::size_t frames = 0;
};

std::vector<AudioClip> SplitIntoClips(const std::vector<float>& audio_pcm,
                                      std::uint32_t sample_rate, std::size_t max_clip_ms);
