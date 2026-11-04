#pragma once

#include "core/IInferenceProvider.h"
#include "core/ISttProvider.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// How the router should decide between the local Whisper stack and the
// configured cloud provider. `api_key_source` is read at call time so the
// gate reflects the live environment instead of a startup snapshot.
struct SttRoutingConfig {
    std::string provider;  // "local" | "gnani" | "elevenlabs" | "muse"
    bool consent_accepted = false;
    std::function<std::string()> api_key_source;
};

// Routes dictation STT between the local stack and a cloud provider.
//
// The engine keeps talking to an IInferenceProvider; this class decides per
// utterance whether the configured cloud provider is allowed (known provider
// + recorded consent + key present in the environment, core/ConsentGate.h).
// A cloud failure, cancellation or missing consent falls back to the local
// provider for that utterance; cleanup (ProcessText) always stays local.
//
// `on_cloud_error` is invoked (before falling back) whenever a cloud attempt
// fails, so the failure is visible to the user rather than silently logged.
class SttRouter final : public IInferenceProvider {
public:
    using CloudErrorCallback = std::function<void(SttErrorCategory, std::string)>;

    SttRouter(std::unique_ptr<IInferenceProvider> local, std::unique_ptr<ISttProvider> cloud,
              SttRoutingConfig config, CloudErrorCallback on_cloud_error = {});
    ~SttRouter() override;

    std::string Transcribe(const std::vector<float>& audio_pcm) override;
    std::string TranscribeWithPrompt(const std::vector<float>& audio_pcm,
                                     const std::string& prompt) override;
    std::string ProcessText(const std::string& text, const std::string& instruction) override;
    void RequestCancel() override;

private:
    bool CloudAllowed() const;

    std::unique_ptr<IInferenceProvider> local_;
    std::unique_ptr<ISttProvider> cloud_;
    SttRoutingConfig config_;
    CloudErrorCallback on_cloud_error_;
    std::atomic<bool> cancel_requested_{false};
};
