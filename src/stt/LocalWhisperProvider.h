#pragma once

#include "core/IInferenceProvider.h"

#include <atomic>
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
    std::string TranscribeWithPrompt(const std::vector<float>& audio_pcm,
                                     const std::string& prompt) override;
    std::string ProcessText(const std::string& text, const std::string& instruction) override;
    void RequestCancel() override;

    void SetPromptPrefix(std::string prefix);

private:
    std::string RunWhisper(const std::vector<float>& audio_pcm, const std::string& prompt);

    whisper_context* ctx_ = nullptr;
    std::atomic<bool> cancel_requested_{false};
    std::string prompt_prefix_;
};
