#include "core/SttRouter.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

void LogAttempts(const SttTranscription& result) {
    for (const SttAttemptStats& attempt : result.attempts) {
        std::cerr << "[stt] provider=" << attempt.provider << " request_id="
                  << (attempt.request_id.empty() ? std::string("-") : attempt.request_id)
                  << " status=" << attempt.http_status << " error=" << ToString(attempt.error)
                  << " bytes=" << attempt.bytes_sent << "/" << attempt.bytes_received
                  << " latency_ms=" << attempt.latency.count() << '\n';
    }
}

}  // namespace

SttRouter::SttRouter(std::unique_ptr<IInferenceProvider> local,
                     std::unique_ptr<ISttProvider> cloud, SttRoutingConfig config)
    : local_(std::move(local)), cloud_(std::move(cloud)), config_(std::move(config)) {
    if (local_ == nullptr) {
        throw std::invalid_argument("SttRouter: local provider is required");
    }
}

void SttRouter::SetCancelCheck(SttCancelCheck cancel_check) {
    cancel_check_ = std::move(cancel_check);
}

const SttTranscription& SttRouter::LastCloudResult() const {
    return last_cloud_;
}

bool SttRouter::CloudSelected() const {
    if (config_.provider != "gnani" || cloud_ == nullptr) {
        return false;
    }
    if (!config_.consent_accepted) {
        return false;
    }
    if (!config_.api_key_source) {
        return false;
    }
    return !config_.api_key_source().empty();
}

std::string SttRouter::Transcribe(const std::vector<float>& audio_pcm) {
    last_cloud_ = SttTranscription{};
    if (!CloudSelected()) {
        if (config_.provider == "gnani") {
            std::cerr << "[stt] provider=local cloud_declined" << '\n';
        }
        return local_->Transcribe(audio_pcm);
    }
    last_cloud_ = cloud_->Transcribe(audio_pcm, cancel_check_);
    LogAttempts(last_cloud_);
    if (last_cloud_.cancelled) {
        return {};
    }
    if (last_cloud_.success) {
        return last_cloud_.text;
    }
    std::cerr << "[stt] provider=local fallback=cloud error=" << ToString(last_cloud_.error)
              << '\n';
    return local_->Transcribe(audio_pcm);
}

std::string SttRouter::ProcessText(const std::string& text, const std::string& instruction) {
    return local_->ProcessText(text, instruction);
}
