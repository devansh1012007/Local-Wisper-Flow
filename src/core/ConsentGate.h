#pragma once

#include <string>
#include <vector>

struct SttConsentInputs {
    std::string provider;
    bool consent_accepted = false;
    bool api_key_present = false;
};

enum class SttConsentDecision {
    CloudAllowed,
    ConsentMissing,
    ApiKeyMissing,
    ProviderUnknown,
};

struct SttConsentVerdict {
    SttConsentDecision decision = SttConsentDecision::ProviderUnknown;
    std::string reason;
};

class ApiKeyStore;

SttConsentVerdict EvaluateCloudConsent(const SttConsentInputs& inputs);

bool IsKnownCloudProvider(const std::string& name);

std::vector<std::string> KnownCloudProviders();

const char* ApiKeyEnvFor(const std::string& name);

std::string ResolveApiKey(const std::string& provider, const ApiKeyStore& store);
