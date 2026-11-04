#pragma once

#include "core/IInferenceProvider.h"

#include <stdexcept>

class UnavailableStt final : public IInferenceProvider {
public:
    std::string Transcribe(const std::vector<float>&) override {
        throw std::runtime_error("speech model is missing or failed to load");
    }

    std::string ProcessText(const std::string& text, const std::string&) override { return text; }
};
