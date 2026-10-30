#include <doctest/doctest.h>

#include "stt/cloud/ClipSplitter.h"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace {

std::vector<float> Speech(std::size_t samples) {
    return std::vector<float>(samples, 0.3f);
}

std::vector<float> SilentSpeechMix(std::size_t total_samples, std::size_t silence_begin,
                                   std::size_t silence_frames) {
    std::vector<float> pcm = Speech(total_samples);
    for (std::size_t i = 0; i < silence_frames && silence_begin + i < total_samples; ++i) {
        pcm[silence_begin + i] = 0.0001f;
    }
    return pcm;
}

}  // namespace

TEST_CASE("clip splitter returns one clip for short audio") {
    const std::vector<float> pcm = Speech(80000);
    const std::vector<AudioClip> clips = SplitIntoClips(pcm, 16000, 29500);
    REQUIRE(clips.size() == 1);
    CHECK(clips[0].begin == 0);
    CHECK(clips[0].frames == 80000);
}

TEST_CASE("clip splitter hard cuts continuous audio at the limit") {
    const std::vector<float> pcm = Speech(1040000);
    const std::vector<AudioClip> clips = SplitIntoClips(pcm, 16000, 29500);
    REQUIRE(clips.size() == 3);
    CHECK(clips[0].begin == 0);
    CHECK(clips[0].frames == 472000);
    CHECK(clips[1].begin == 472000);
    CHECK(clips[1].frames == 472000);
    CHECK(clips[2].begin == 944000);
    CHECK(clips[2].frames == 96000);
}

TEST_CASE("clip splitter prefers silence near the boundary") {
    const std::vector<float> pcm = SilentSpeechMix(640000, 448000, 512);
    const std::vector<AudioClip> clips = SplitIntoClips(pcm, 16000, 29500);
    REQUIRE(clips.size() == 2);
    CHECK(clips[0].begin == 0);
    CHECK(clips[0].frames == 448512);
    CHECK(clips[1].begin == 448512);
    CHECK(clips[1].frames == 191488);
}

TEST_CASE("clip splitter ignores silence far from the boundary") {
    const std::vector<float> pcm = SilentSpeechMix(640000, 320000, 512);
    const std::vector<AudioClip> clips = SplitIntoClips(pcm, 16000, 29500);
    REQUIRE(clips.size() == 2);
    CHECK(clips[0].begin == 0);
    CHECK(clips[0].frames == 472000);
    CHECK(clips[1].begin == 472000);
    CHECK(clips[1].frames == 168000);
}

TEST_CASE("clip splitter returns no clips for empty audio") {
    const std::vector<float> pcm;
    const std::vector<AudioClip> clips = SplitIntoClips(pcm, 16000, 29500);
    CHECK(clips.empty());
}

TEST_CASE("clip splitter rejects zero limits") {
    const std::vector<float> pcm = Speech(1000);
    CHECK_THROWS_AS(SplitIntoClips(pcm, 0, 29500), std::runtime_error);
    CHECK_THROWS_AS(SplitIntoClips(pcm, 16000, 0), std::runtime_error);
}
