#pragma once

#include <string>
#include <vector>

class IInferenceProvider {
public:
    virtual ~IInferenceProvider() = default;

    virtual std::string Transcribe(const std::vector<float>& audio_pcm) = 0;
    virtual std::string ProcessText(const std::string& text, const std::string& instruction) = 0;
};
