#include "stt/cloud/ClipSplitter.h"

#include <stdexcept>

namespace {

constexpr std::size_t kWindowSamples = 512;
constexpr float kSilenceThreshold = 0.002f;
constexpr std::size_t kBacktrackSeconds = 2;

bool WindowIsSilent(const float* samples) {
    for (std::size_t i = 0; i < kWindowSamples; ++i) {
        if (samples[i] > kSilenceThreshold || samples[i] < -kSilenceThreshold) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::vector<AudioClip> SplitIntoClips(const std::vector<float>& audio_pcm,
                                      std::uint32_t sample_rate, std::size_t max_clip_ms) {
    if (sample_rate == 0) {
        throw std::runtime_error("ClipSplitter: sample rate must be positive");
    }
    if (max_clip_ms == 0) {
        throw std::runtime_error("ClipSplitter: max clip duration must be positive");
    }
    std::vector<AudioClip> clips;
    const std::size_t total = audio_pcm.size();
    if (total == 0) {
        return clips;
    }
    const std::size_t limit = max_clip_ms * sample_rate / 1000;
    const std::size_t backtrack = kBacktrackSeconds * sample_rate;
    std::size_t begin = 0;
    while (begin < total) {
        const std::size_t end = begin + limit;
        if (end >= total) {
            clips.push_back({begin, total - begin});
            break;
        }
        std::size_t cut = end;
        std::size_t search_floor = end > backtrack ? end - backtrack : 0;
        if (search_floor < begin) {
            search_floor = begin;
        }
        for (std::size_t window_end = end / kWindowSamples * kWindowSamples;
             window_end > search_floor && window_end > begin;
             window_end -= kWindowSamples) {
            if (WindowIsSilent(audio_pcm.data() + (window_end - kWindowSamples))) {
                cut = window_end;
                break;
            }
        }
        clips.push_back({begin, cut - begin});
        begin = cut;
    }
    return clips;
}
