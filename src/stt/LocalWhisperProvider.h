#pragma once

#include "core/IInferenceProvider.h"

#include <string>
#include <vector>

struct whisper_context;

class LocalWhisperProvider : public IInferenceProvider {
public:
    explicit LocalWhisperProvider(const std::string& model_path);
    ~LocalWhisperProvider() override;

    LocalWhisperProvider(const LocalWhisperProvider&) = delete;
    LocalWhisperProvider& operator=(const LocalWhisperProvider&) = delete;

    std::string Transcribe(const std::vector<float>& audio_pcm) override;
    std::string ProcessText(const std::string& text, const std::string& instruction) override;

private:
    whisper_context* ctx_ = nullptr;
};
