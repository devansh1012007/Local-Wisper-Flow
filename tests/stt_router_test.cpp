#include <doctest/doctest.h>

#include "core/SttRouter.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class ScriptedLocal final : public IInferenceProvider {
public:
    ScriptedLocal(std::string tag, bool fail) : tag_(std::move(tag)), fail_(fail) {}

    std::string Transcribe(const std::vector<float>&) override {
        ++transcribe_calls;
        if (fail_) {
            throw std::runtime_error("local failure");
        }
        return "local:" + tag_;
    }

    std::string ProcessText(const std::string& text, const std::string&) override {
        ++process_calls;
        return "processed:" + text;
    }

    int transcribe_calls = 0;
    int process_calls = 0;

private:
    std::string tag_;
    bool fail_;
};

class ScriptedCloud final : public ISttProvider {
public:
    SttTranscription Transcribe(const std::vector<float>& audio_pcm,
                                const SttCancelCheck& cancel_check) override {
        ++calls;
        last_audio_size = audio_pcm.size();
        received_cancel_check = static_cast<bool>(cancel_check);
        if (cancel_check) {
            (void)cancel_check();
        }
        SttTranscription result;
        result.text = outcome_text;
        result.success = outcome_success;
        result.cancelled = outcome_cancelled;
        result.error = outcome_error;
        SttAttemptStats attempt;
        attempt.provider = "gnani";
        attempt.http_status = outcome_status;
        attempt.bytes_sent = 1024;
        attempt.bytes_received = 256;
        attempt.latency = std::chrono::milliseconds(42);
        result.attempts.push_back(attempt);
        return result;
    }

    std::string outcome_text = "cloud:text";
    bool outcome_success = true;
    bool outcome_cancelled = false;
    SttErrorCategory outcome_error = SttErrorCategory::None;
    int outcome_status = 200;
    int calls = 0;
    std::size_t last_audio_size = 0;
    bool received_cancel_check = false;
};

SttRoutingConfig CloudConfig() {
    SttRoutingConfig config;
    config.provider = "gnani";
    config.consent_accepted = true;
    config.api_key_source = [] { return std::string("key-present"); };
    return config;
}

}  // namespace

TEST_CASE("router stays local by default") {
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), SttRoutingConfig{});
    const std::vector<float> pcm(16, 0.0f);
    CHECK(router.Transcribe(pcm) == "local:l");
    CHECK(cloud->calls == 0);
}

TEST_CASE("router uses cloud when opted in with consent and a key") {
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    const std::vector<float> pcm(32, 0.0f);
    CHECK(router.Transcribe(pcm) == "cloud:text");
    CHECK(cloud->calls == 1);
    CHECK(cloud->last_audio_size == 32);
}

TEST_CASE("router refuses cloud without consent") {
    SttRoutingConfig config = CloudConfig();
    config.consent_accepted = false;
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), config);
    CHECK(router.Transcribe({16, 0.0f}) == "local:l");
    CHECK(cloud->calls == 0);
}

TEST_CASE("router refuses cloud without an api key") {
    SttRoutingConfig config = CloudConfig();
    config.api_key_source = [] { return std::string(); };
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), config);
    CHECK(router.Transcribe({16, 0.0f}) == "local:l");
    CHECK(cloud->calls == 0);
}

TEST_CASE("router accepts a null cloud provider") {
    SttRoutingConfig config = CloudConfig();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false), nullptr, config);
    CHECK(router.Transcribe({16, 0.0f}) == "local:l");
}

TEST_CASE("router falls back to local on cloud failure") {
    ScriptedCloud* cloud = new ScriptedCloud();
    cloud->outcome_success = false;
    cloud->outcome_error = SttErrorCategory::Auth;
    cloud->outcome_text.clear();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    CHECK(router.Transcribe({16, 0.0f}) == "local:l");
    CHECK(cloud->calls == 1);
    CHECK(router.LastCloudResult().error == SttErrorCategory::Auth);
}

TEST_CASE("router never calls local when cloud succeeds") {
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", true),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    CHECK(router.Transcribe({16, 0.0f}) == "cloud:text");
}

TEST_CASE("router does not fall back when cloud returns empty text") {
    ScriptedCloud* cloud = new ScriptedCloud();
    cloud->outcome_text.clear();
    SttRouter router(std::make_unique<ScriptedLocal>("l", true),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    CHECK(router.Transcribe({16, 0.0f}).empty());
    CHECK(cloud->calls == 1);
}

TEST_CASE("router does not fall back when cloud was cancelled") {
    ScriptedCloud* cloud = new ScriptedCloud();
    cloud->outcome_cancelled = true;
    cloud->outcome_text.clear();
    cloud->outcome_error = SttErrorCategory::Cancelled;
    SttRouter router(std::make_unique<ScriptedLocal>("l", true),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    CHECK(router.Transcribe({16, 0.0f}).empty());
    CHECK(cloud->calls == 1);
}

TEST_CASE("router forwards the cancel check to the cloud provider") {
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    bool flag = false;
    router.SetCancelCheck([&flag] { return flag; });
    router.Transcribe({16, 0.0f});
    CHECK(cloud->received_cancel_check);
    flag = true;
    router.Transcribe({16, 0.0f});
}

TEST_CASE("router routes text processing to the local provider") {
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    CHECK(router.ProcessText("hi", "prompt") == "processed:hi");
    CHECK(cloud->calls == 0);
}

TEST_CASE("router records cloud attempt statistics") {
    ScriptedCloud* cloud = new ScriptedCloud();
    SttRouter router(std::make_unique<ScriptedLocal>("l", false),
                     std::unique_ptr<ISttProvider>(cloud), CloudConfig());
    router.Transcribe({16, 0.0f});
    const SttTranscription& last = router.LastCloudResult();
    REQUIRE(last.attempts.size() == 1);
    CHECK(last.attempts[0].provider == "gnani");
    CHECK(last.attempts[0].http_status == 200);
    CHECK(last.attempts[0].bytes_sent == 1024);
    CHECK(last.attempts[0].bytes_received == 256);
    CHECK(last.attempts[0].latency.count() == 42);
}

TEST_CASE("router requires a local provider") {
    CHECK_THROWS_AS(SttRouter router(nullptr, nullptr, SttRoutingConfig{}),
                    std::invalid_argument);
}
