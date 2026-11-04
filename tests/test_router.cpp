#include "core/ILlmProvider.h"
#include "core/IInferenceProvider.h"
#include "core/ProviderRouter.h"

#include <doctest/doctest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Scripted final : public IInferenceProvider {
public:
    Scripted(std::string tag, bool fail) : tag_(std::move(tag)), fail_(fail) {}

    std::string Transcribe(const std::vector<float>&) override {
        if (fail_) {
            throw std::runtime_error("scripted failure");
        }
        return "T:" + tag_;
    }

    std::string ProcessText(const std::string& text, const std::string&) override {
        if (fail_) {
            throw std::runtime_error("scripted failure");
        }
        return "P:" + text + ":" + tag_;
    }

private:
    std::string tag_;
    bool fail_;
};

class StackLlm final : public ILlmProvider {
public:
    explicit StackLlm(IInferenceProvider& provider) : provider_(provider) {}

    std::string Process(const std::string& text, const std::string& prompt) override {
        return provider_.ProcessText(text, prompt);
    }

private:
    IInferenceProvider& provider_;
};

class LocalStack final : public IInferenceProvider {
public:
    LocalStack(IInferenceProvider& stt, ILlmProvider& llm) : stt_(stt), llm_(llm) {}

    std::string Transcribe(const std::vector<float>& pcm) override { return stt_.Transcribe(pcm); }

    std::string ProcessText(const std::string& text, const std::string& prompt) override {
        return llm_.Process(text, prompt);
    }

private:
    IInferenceProvider& stt_;
    ILlmProvider& llm_;
};

}  // namespace

TEST_CASE("router falls back to local when remote fails") {
    ProviderRouter router(std::make_unique<Scripted>("local", false),
                          std::make_unique<Scripted>("remote", true), true);
    const std::vector<float> pcm(16, 0.0f);
    CHECK(router.Transcribe(pcm) == "T:local");
    CHECK(router.ProcessText("hi", "") == "P:hi:local");
}

TEST_CASE("router uses local when use_remote is false") {
    ProviderRouter router(std::make_unique<Scripted>("local", false),
                          std::make_unique<Scripted>("remote", false), false);
    CHECK(router.ProcessText("hi", "") == "P:hi:local");
}

TEST_CASE("router uses remote first when configured") {
    ProviderRouter router(std::make_unique<Scripted>("local", false),
                          std::make_unique<Scripted>("remote", false), true);
    const std::vector<float> pcm(16, 0.0f);
    CHECK(router.Transcribe(pcm) == "T:remote");
}

TEST_CASE("router rejects null local provider") {
    CHECK_THROWS_AS(ProviderRouter(nullptr, std::make_unique<Scripted>("r", false), false),
                    std::invalid_argument);
    CHECK_THROWS_AS(ProviderRouter(std::make_unique<Scripted>("l", false), nullptr, true),
                    std::invalid_argument);
}

TEST_CASE("local provider stack routes stt to stt and process to llm") {
    Scripted stt("stt", false);
    Scripted llm_side("llm", false);
    StackLlm llm(llm_side);
    LocalStack stack(stt, llm);
    const std::vector<float> pcm(8, 0.0f);
    CHECK(stack.Transcribe(pcm) == "T:stt");
    CHECK(stack.ProcessText("hi", "") == "P:hi:llm");
}
