#include <doctest/doctest.h>

#include "core/ConsentGate.h"

#include <string>
#include <vector>

TEST_CASE("gate allows cloud only with a known provider, consent, and a key") {
    SttConsentInputs inputs;
    inputs.provider = "gnani";
    inputs.consent_accepted = true;
    inputs.api_key_present = true;
    const SttConsentVerdict verdict = EvaluateCloudConsent(inputs);
    CHECK(verdict.decision == SttConsentDecision::CloudAllowed);
}

TEST_CASE("gate refuses cloud without consent") {
    SttConsentInputs inputs;
    inputs.provider = "gnani";
    inputs.consent_accepted = false;
    inputs.api_key_present = true;
    const SttConsentVerdict verdict = EvaluateCloudConsent(inputs);
    CHECK(verdict.decision == SttConsentDecision::ConsentMissing);
    CHECK(verdict.reason == "stt_consent_accepted is false");
}

TEST_CASE("gate refuses cloud without the provider key and names the env variable") {
    SttConsentInputs inputs;
    inputs.provider = "gnani";
    inputs.consent_accepted = true;
    inputs.api_key_present = false;
    const SttConsentVerdict verdict = EvaluateCloudConsent(inputs);
    CHECK(verdict.decision == SttConsentDecision::ApiKeyMissing);
    CHECK(verdict.reason.find("GNANI_API_KEY") != std::string::npos);
    CHECK(verdict.reason.find("is not set") != std::string::npos);
}

TEST_CASE("gate rejects unknown providers without consulting consent") {
    for (const std::string& provider : std::vector<std::string>{"local", "", "not-a-provider",
                                                                 "GNANI"}) {
        SttConsentInputs inputs;
        inputs.provider = provider;
        inputs.consent_accepted = true;
        inputs.api_key_present = true;
        const SttConsentVerdict verdict = EvaluateCloudConsent(inputs);
        CHECK(verdict.decision == SttConsentDecision::ProviderUnknown);
    }
    SttConsentInputs inputs;
    inputs.provider = "not-a-provider";
    const SttConsentVerdict verdict = EvaluateCloudConsent(inputs);
    CHECK(verdict.reason.find("not-a-provider") != std::string::npos);
}

TEST_CASE("every registered provider allows cloud with consent and a key") {
    for (const std::string& provider : KnownCloudProviders()) {
        SttConsentInputs inputs;
        inputs.provider = provider;
        inputs.consent_accepted = true;
        inputs.api_key_present = true;
        CHECK(EvaluateCloudConsent(inputs).decision == SttConsentDecision::CloudAllowed);
        SttConsentInputs missing;
        missing.provider = provider;
        missing.consent_accepted = true;
        missing.api_key_present = false;
        const SttConsentVerdict verdict = EvaluateCloudConsent(missing);
        CHECK(verdict.decision == SttConsentDecision::ApiKeyMissing);
        CHECK(verdict.reason.find("is not set") != std::string::npos);
    }
}

TEST_CASE("every known cloud provider resolves to an api key env variable") {
    const std::vector<std::string> providers = KnownCloudProviders();
    CHECK(providers.size() >= 1);
    bool gnani_listed = false;
    for (const std::string& provider : providers) {
        CHECK(IsKnownCloudProvider(provider));
        CHECK(ApiKeyEnvFor(provider) != nullptr);
        if (provider == "gnani") {
            gnani_listed = true;
        }
    }
    CHECK(gnani_listed);
}

TEST_CASE("api key env lookup is null for unknown providers") {
    CHECK(ApiKeyEnvFor("local") == nullptr);
    CHECK(ApiKeyEnvFor(std::string()) == nullptr);
}
