#pragma once

#include "core/IVadProvider.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class SileroVadProvider : public IVadProvider {
public:
    static constexpr std::size_t kRequiredChunkSamples = 512;

    SileroVadProvider(const std::string& model_path, float speech_threshold);
    ~SileroVadProvider() override;

    SileroVadProvider(const SileroVadProvider&) = delete;
    SileroVadProvider& operator=(const SileroVadProvider&) = delete;

    bool IsSpeech(const std::vector<float>& audio_chunk) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::mutex mutex_;
    float threshold_;
};
