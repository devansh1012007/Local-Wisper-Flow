#include "core/ILlmProvider.h"
#include "core/TransformManager.h"
#include "V0ConfigFixture.h"

#include <doctest/doctest.h>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

class PassthroughLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override {
        std::string trimmed;
        std::size_t begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return {};
        }
        std::size_t end = text.find_last_not_of(" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }
};

void WaitFor(std::atomic<bool>& flag) {
    for (int i = 0; i < 500 && !flag.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace

TEST_CASE("transform manager runs async and trims") {
    SeedTestDataFiles();
    PassthroughLlm llm;
    TransformManager manager(llm, "transforms.json");

    std::atomic<bool> done{false};
    std::string result;
    manager.ExecuteTransform("Fix Grammar", "  helo wrld  ", [&](std::string value) {
        result = value;
        done.store(true);
    });
    WaitFor(done);

    CHECK(done.load());
    CHECK(result == "helo wrld");
}

TEST_CASE("unknown transform yields empty result") {
    SeedTestDataFiles();
    PassthroughLlm llm;
    TransformManager manager(llm, "transforms.json");

    std::atomic<bool> done{false};
    std::string result = "sentinel";
    manager.ExecuteTransform("Does Not Exist", "abc", [&](std::string value) {
        result = value;
        done.store(true);
    });
    WaitFor(done);

    CHECK(done.load());
    CHECK(result.empty());
}

TEST_CASE("empty text yields empty result without calling provider") {
    SeedTestDataFiles();
    PassthroughLlm llm;
    TransformManager manager(llm, "transforms.json");

    std::atomic<bool> done{false};
    manager.ExecuteTransform("Fix Grammar", "   ", [&](std::string value) {
        CHECK(value.empty());
        done.store(true);
    });
    WaitFor(done);
    CHECK(done.load());
}

TEST_CASE("missing transforms file throws") {
    PassthroughLlm llm;
    CHECK_THROWS_AS(TransformManager(llm, "no_such_transforms.json"), std::runtime_error);
}
