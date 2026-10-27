#include <doctest/doctest.h>

#include "core/ProviderRouter.h"

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

}  // namespace

TEST_CASE("router falls back to local when remote fails") {
    ProviderRouter router(std::make_unique<Scripted>("local", false),
                          std::make_unique<Scripted>("remote", true), true);
    const std::vector<float> pcm(16, 0.0f);
    const std::string transcript = router.Transcribe(pcm);
    const std::string processed = router.ProcessText("hi", "");
    CHECK(transcript == "T:local");
    CHECK(processed == "P:hi:local");
}

TEST_CASE("router prefers remote when healthy") {
    ProviderRouter router(std::make_unique<Scripted>("local", false),
                          std::make_unique<Scripted>("remote", false), true);
    const std::vector<float> pcm(16, 0.0f);
    CHECK(router.Transcribe(pcm) == "T:remote");
    CHECK(router.ProcessText("hi", "") == "P:hi:remote");
}

TEST_CASE("router uses local when use_remote is false") {
    ProviderRouter router(std::make_unique<Scripted>("local", false),
                          std::make_unique<Scripted>("remote", false), false);
    CHECK(router.ProcessText("hi", "") == "P:hi:local");
}

TEST_CASE("router accepts a null remote when use_remote is false") {
    ProviderRouter router(std::make_unique<Scripted>("local", false), nullptr, false);
    const std::vector<float> pcm(16, 0.0f);
    CHECK(router.Transcribe(pcm) == "T:local");
}
