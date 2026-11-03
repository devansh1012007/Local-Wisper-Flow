#include "core/ConsentGate.h"

#include <cstddef>
#include <utility>

namespace {

struct CloudProviderInfo {
    const char* name;
    const char* api_key_env;
};

const CloudProviderInfo kCloudProviders[] = {{"gnani", "GNANI_API_KEY"}};
constexpr std::size_t kCloudProviderCount = 1;

const CloudProviderInfo* FindProvider(const std::string& name) {
    for (std::size_t i = 0; i < kCloudProviderCount; ++i) {
        if (name == kCloudProviders[i].name) {
            return &kCloudProviders[i];
        }
    }
    return nullptr;
}

}  // namespace

bool IsKnownCloudProvider(const std::string& name) {
    return FindProvider(name) != nullptr;
}

std::vector<std::string> KnownCloudProviders() {
    std::vector<std::string> names;
    names.reserve(kCloudProviderCount);
    for (std::size_t i = 0; i < kCloudProviderCount; ++i) {
        names.emplace_back(kCloudProviders[i].name);
    }
    return names;
}

const char* ApiKeyEnvFor(const std::string& name) {
    const CloudProviderInfo* info = FindProvider(name);
    return info != nullptr ? info->api_key_env : nullptr;
}

SttConsentVerdict EvaluateCloudConsent(const SttConsentInputs& inputs) {
    const CloudProviderInfo* info = FindProvider(inputs.provider);
    if (info == nullptr) {
        SttConsentVerdict verdict;
        verdict.decision = SttConsentDecision::ProviderUnknown;
        verdict.reason = "unknown stt_provider '" + inputs.provider + "'";
        return verdict;
    }
    if (!inputs.consent_accepted) {
        SttConsentVerdict verdict;
        verdict.decision = SttConsentDecision::ConsentMissing;
        verdict.reason = "stt_consent_accepted is false";
        return verdict;
    }
    if (!inputs.api_key_present) {
        SttConsentVerdict verdict;
        verdict.decision = SttConsentDecision::ApiKeyMissing;
        verdict.reason = std::string(info->api_key_env) + " is not set";
        return verdict;
    }
    SttConsentVerdict verdict;
    verdict.decision = SttConsentDecision::CloudAllowed;
    verdict.reason = "cloud transcription enabled";
    return verdict;
}
