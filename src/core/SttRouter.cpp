#include "core/SttRouter.h"

#include "core/ConsentGate.h"
#include "core/Log.h"

#include <chrono>
#include <thread>
#include <utility>

SttRouter::SttRouter(std::unique_ptr<IInferenceProvider> local,
                     std::unique_ptr<ISttProvider> cloud, SttRoutingConfig config,
                     CloudErrorCallback on_cloud_error)
    : local_(std::move(local)), cloud_(std::move(cloud)), config_(std::move(config)),
      on_cloud_error_(std::move(on_cloud_error)) {}

SttRouter::~SttRouter() = default;

bool SttRouter::CloudAllowed() const {
    if (config_.provider == "local" || cloud_ == nullptr) {
        return false;
    }
    if (!IsKnownCloudProvider(config_.provider)) {
        return false;
    }
    SttConsentInputs inputs;
    inputs.provider = config_.provider;
    inputs.consent_accepted = config_.consent_accepted;
    inputs.api_key_present =
        config_.api_key_source != nullptr && !config_.api_key_source().empty();
    return EvaluateCloudConsent(inputs).decision == SttConsentDecision::CloudAllowed;
}

std::string SttRouter::Transcribe(const std::vector<float>& audio_pcm) {
    return TranscribeWithPrompt(audio_pcm, std::string());
}

std::string SttRouter::TranscribeWithPrompt(const std::vector<float>& audio_pcm,
                                            const std::string& prompt) {
    cancel_requested_.store(false, std::memory_order_relaxed);
    if (CloudAllowed()) {
        // Retry on rate-limited (429) before falling back to local.
        int retries = 0;
        const int max_retries = 2;
        for (;;) {
            const SttTranscription result =
                cloud_->Transcribe(audio_pcm, [this] { return cancel_requested_.load(); });
            if (result.cancelled) {
                // A cancelled dictation is not a cloud failure: surface it as an
                // empty transcript instead of re-running the local model.
                return {};
            }
            if (result.success) {
                return result.text;
            }
            // Surface the cloud failure visibly before deciding whether to retry
            // or fall back.
            if (on_cloud_error_) {
                std::string detail;
                if (!result.attempts.empty()) {
                    detail = result.attempts.back().detail;
                }
                on_cloud_error_(result.error, detail);
            }
            if (result.error == SttErrorCategory::RateLimited && retries < max_retries) {
                ++retries;
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * retries));
                continue;
            }
            lwf::Log::Warning("cloud stt failed (" + std::string(ToString(result.error)) +
                              "); falling back to local whisper for this utterance");
            break;
        }
    }
    if (prompt.empty()) {
        return local_->Transcribe(audio_pcm);
    }
    return local_->TranscribeWithPrompt(audio_pcm, prompt);
}

std::string SttRouter::ProcessText(const std::string& text, const std::string& instruction) {
    // Cleanup and spoken commands never leave the machine; only STT routes.
    return local_->ProcessText(text, instruction);
}

void SttRouter::RequestCancel() {
    cancel_requested_.store(true, std::memory_order_relaxed);
    local_->RequestCancel();
}
