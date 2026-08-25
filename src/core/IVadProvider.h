#pragma once

#include <vector>

class IVadProvider {
public:
    virtual ~IVadProvider() = default;

    virtual bool IsSpeech(const std::vector<float>& audio_chunk) = 0;
};
