#include <doctest/doctest.h>

#include "core/SttRouter.h"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace {

class LocalFake final : public IInferenceProvider {
public:
    std::string Transcribe(const std::vector<float>&) override {
        ++transcribe_calls;
        return "local";
    }

    std::string TranscribeWithPrompt(const std::vector<float>&, const std::string& prompt) override {
        ++prompt_calls;
        last_prompt = prompt;
        return "local:" + prompt;
    }

    std::string ProcessText(const std::string& text, const std::string&) override {
        ++process_calls;
        return "processed:" + text;
    }

    int transcribe_calls = 0;
    int prompt_calls = 0;
    int process_calls = 0;
    std::string last_prompt;
};

class CloudFake final : public ISttProvider {
public:
    SttTranscription Transcribe(const std::vector<float>&, const SttCancelCheck& check) override {
        ++calls;
        last_cancel_check = check;
        if (script) {
            return script();
        }
        SttTranscription result;
        result.success = true;
        result.text = "cloud";
        return result;
    }

    int calls = 0;
    SttCancelCheck last_cancel_check;
    std::function<SttTranscription()> script;
};

SttRoutingConfig Routing(std::string provider, bool consent, std::string key) {
    SttRoutingConfig config;
    config.provider = std::move(provider);
    config.consent_accepted = consent;
    config.api_key_source = [key] { return key; };
    return config;
}

struct RouterHarness {
    // Raw observers; the router owns the instances.
    LocalFake* local;
    CloudFake* cloud;
    SttRouter router;

    RouterHarness(const std::string& provider, bool consent, const std::string& key)
        : local(new LocalFake()), cloud(new CloudFake()),
          router(std::unique_ptr<IInferenceProvider>(local),
                 std::unique_ptr<ISttProvider>(cloud), Routing(provider, consent, key)) {}
};

}  // namespace

TEST_CASE("router sends the utterance to the cloud provider when the gate allows it") {
    RouterHarness h("gnani", true, "key-1");
    CHECK(h.router.Transcribe(std::vector<float>(1600, 0.1f)) == "cloud");
    CHECK(h.cloud->calls == 1);
    CHECK(h.local->transcribe_calls == 0);
}

TEST_CASE("router falls back to local without consent or key or a known provider") {
    for (const auto& [provider, consent, key] :
         std::vector<std::tuple<std::string, bool, std::string>>{
             {"gnani", false, "key-1"}, {"gnani", true, ""}, {"not-a-provider", true, "key-1"}}) {
        RouterHarness h(provider, consent, key);
        CHECK(h.router.Transcribe({}) == "local");
        CHECK(h.cloud->calls == 0);
        CHECK(h.local->transcribe_calls == 1);
    }
}

TEST_CASE("router treats provider=local as local even with a configured cloud stack") {
    RouterHarness h("local", true, "key-1");
    CHECK(h.router.Transcribe({}) == "local");
    CHECK(h.cloud->calls == 0);
}

TEST_CASE("router falls back to local after a cloud failure") {
    RouterHarness h("gnani", true, "key-1");
    h.cloud->script = [] {
        SttTranscription failure;
        failure.success = false;
        failure.error = SttErrorCategory::Auth;
        return failure;
    };
    CHECK(h.router.Transcribe(std::vector<float>(1600, 0.1f)) == "local");
    CHECK(h.cloud->calls == 1);
    CHECK(h.local->transcribe_calls == 1);
}

TEST_CASE("router returns an empty transcript for a cancelled cloud utterance") {
    RouterHarness h("gnani", true, "key-1");
    h.cloud->script = [] {
        SttTranscription cancelled;
        cancelled.cancelled = true;
        cancelled.error = SttErrorCategory::Cancelled;
        return cancelled;
    };
    CHECK(h.router.Transcribe(std::vector<float>(1600, 0.1f)).empty());
    CHECK(h.local->transcribe_calls == 0);
}

TEST_CASE("router forwards RequestCancel to the cloud cancel check") {
    RouterHarness h("gnani", true, "key-1");
    (void)h.router.Transcribe(std::vector<float>(1600, 0.1f));
    REQUIRE(h.cloud->last_cancel_check != nullptr);
    CHECK_FALSE(h.cloud->last_cancel_check());
    // RequestCancel fires the same check the in-flight cloud call polls.
    h.router.RequestCancel();
    CHECK(h.cloud->last_cancel_check());
}

TEST_CASE("cleanup always stays on the local stack") {
    RouterHarness h("gnani", true, "key-1");
    CHECK(h.router.ProcessText("hello", "") == "processed:hello");
    CHECK(h.cloud->calls == 0);
    CHECK(h.local->process_calls == 1);
}

TEST_CASE("the streaming prompt reaches local whisper on the local path") {
    RouterHarness h("local", false, "");
    CHECK(h.router.TranscribeWithPrompt(std::vector<float>(1600, 0.1f), "ctx") == "local:ctx");
    CHECK(h.local->last_prompt == "ctx");
    CHECK(h.cloud->calls == 0);
}
