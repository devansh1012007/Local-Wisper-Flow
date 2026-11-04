#include "stt/cloud/ClipSplitter.h"

#include <algorithm>

std::vector<AudioClip> SplitIntoClips(const std::vector<float>& pcm, std::uint32_t sample_rate,
                                      std::size_t max_clip_ms) {
    std::vector<AudioClip> clips;
    if (pcm.empty() || sample_rate == 0 || max_clip_ms == 0) {
        return clips;
    }
    const std::size_t clip_frames =
        std::max<std::size_t>(1, static_cast<std::size_t>(max_clip_ms * sample_rate / 1000));
    for (std::size_t begin = 0; begin < pcm.size(); begin += clip_frames) {
        const std::size_t frames = std::min(clip_frames, pcm.size() - begin);
        clips.push_back(AudioClip{begin, frames});
    }
    return clips;
}
