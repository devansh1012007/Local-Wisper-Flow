#pragma once

#include "core/IInferenceProvider.h"
#include "core/ISttProvider.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SttRoutingConfig {
    std::string provider = "local";
    bool consent_accepted = false;
    std::function<std::string()> api_key_source;
};

class SttRouter final : public IInferenceProvider {
public:
    SttRouter(std::unique_ptr<IInferenceProvider> local, std::unique_ptr<ISttProvider> cloud,
              SttRoutingConfig config);

    std::string Transcribe(const std::vector<float>& audio_pcm) override;
    std::string ProcessText(const std::string& text, const std::string& instruction) override;

    void SetCancelCheck(SttCancelCheck cancel_check);

    const SttTranscription& LastCloudResult() const;

private:
    bool CloudSelected() const;

    std::unique_ptr<IInferenceProvider> local_;
    std::unique_ptr<ISttProvider> cloud_;
    SttRoutingConfig config_;
    SttCancelCheck cancel_check_;
    SttTranscription last_cloud_;
};
