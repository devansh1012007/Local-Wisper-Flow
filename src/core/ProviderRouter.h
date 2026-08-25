#pragma once

#include "core/IInferenceProvider.h"

#include <memory>
#include <string>
#include <vector>

class ProviderRouter : public IInferenceProvider {
public:
    ProviderRouter(std::unique_ptr<IInferenceProvider> local,
                   std::unique_ptr<IInferenceProvider> remote, bool use_remote);

    std::string Transcribe(const std::vector<float>& audio_pcm) override;
    std::string ProcessText(const std::string& text,
                            const std::string& system_prompt) override;

private:
    IInferenceProvider& Local() const;

    std::unique_ptr<IInferenceProvider> local_;
    std::unique_ptr<IInferenceProvider> remote_;
    bool use_remote_;
};
