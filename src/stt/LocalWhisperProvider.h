#pragma once

#include "core/IInferenceProvider.h"

#include <mutex>
#include <string>
#include <vector>

struct whisper_context;

struct WhisperSettings {
    std::string language = "auto";
    int threads = 4;
    int beam_size = 1;
    bool suppress_non_speech_tokens = true;
    bool disable_temperature_fallback = false;
};

class LocalWhisperProvider : public IInferenceProvider {
public:
    explicit LocalWhisperProvider(const std::string& model_path, WhisperSettings settings);
    ~LocalWhisperProvider() override;

    LocalWhisperProvider(const LocalWhisperProvider&) = delete;
    LocalWhisperProvider& operator=(const LocalWhisperProvider&) = delete;

    std::string Transcribe(const std::vector<float>& audio_pcm) override;
    std::string ProcessText(const std::string& text, const std::string& instruction) override;

    void SetInitialPrompt(const std::string& prompt);

    void WarmUp();

private:
    whisper_context* ctx_ = nullptr;
    WhisperSettings settings_;
    std::mutex mutex_;
    std::string initial_prompt_;
};
