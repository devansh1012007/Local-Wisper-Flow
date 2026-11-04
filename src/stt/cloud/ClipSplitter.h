#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// A half-open slice [begin, begin + frames) of the source PCM buffer.
struct AudioClip {
    std::size_t begin = 0;
    std::size_t frames = 0;
};

// Splits pcm into consecutive clips of at most max_clip_ms milliseconds
// (at the given sample rate). An empty buffer yields no clips; the last
// clip is the short remainder.
std::vector<AudioClip> SplitIntoClips(const std::vector<float>& pcm, std::uint32_t sample_rate,
                                      std::size_t max_clip_ms);
