#include "core/ProviderRouter.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

template <typename Call>
std::string Route(Call&& call, IInferenceProvider* remote, IInferenceProvider& local,
                  bool use_remote, const char* task) {
    if (!use_remote || remote == nullptr) {
        return call(local);
    }
    try {
        return call(*remote);
    } catch (const std::exception& e) {
        std::cerr << "[router] remote " << task << " failed (" << e.what()
                  << "), falling back to local" << std::endl;
        return call(local);
    }
}

}  // namespace

ProviderRouter::ProviderRouter(std::unique_ptr<IInferenceProvider> local,
                               std::unique_ptr<IInferenceProvider> remote, bool use_remote)
    : local_(std::move(local)), remote_(std::move(remote)), use_remote_(use_remote) {
    if (local_ == nullptr) {
        throw std::invalid_argument("ProviderRouter: local provider is required");
    }
    if (use_remote_ && remote_ == nullptr) {
        throw std::invalid_argument("ProviderRouter: use_remote requires a remote provider");
    }
}

IInferenceProvider& ProviderRouter::Local() const {
    return *local_;
}

std::string ProviderRouter::Transcribe(const std::vector<float>& audio_pcm) {
    return Route(
        [this, &audio_pcm](IInferenceProvider& provider) { return provider.Transcribe(audio_pcm); },
        remote_.get(), Local(), use_remote_, "transcription");
}

std::string ProviderRouter::ProcessText(const std::string& text,
                                        const std::string& system_prompt) {
    return Route(
        [this, &text, &system_prompt](IInferenceProvider& provider) {
            return provider.ProcessText(text, system_prompt);
        },
        remote_.get(), Local(), use_remote_, "text processing");
}
