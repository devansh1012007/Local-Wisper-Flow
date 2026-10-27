#include <doctest/doctest.h>

#include "core/TransformManager.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "fakes.h"

namespace {

const char* kTransformsPath = "tests/data/transforms_test.json";

void WriteTransformsFile() {
    std::filesystem::create_directories("tests/data");
    std::ofstream file(kTransformsPath);
    file << R"([
        {"name": "Fix Grammar", "prompt": "Fix this: {text}"},
        {"name": "Shout", "prompt": "Shout this: {text}"}
    ])";
}

}  // namespace

TEST_CASE("transform manager executes a named transform on a worker") {
    WriteTransformsFile();
    ScriptedLlm llm;
    TransformManager manager(llm, kTransformsPath);

    std::atomic<bool> done{false};
    std::string observed;
    manager.ExecuteTransform("Fix Grammar", "  helo wrld  ", [&](std::string result) {
        observed = result;
        done.store(true);
    });

    for (int i = 0; i < 200 && !done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(done.load());
    CHECK(observed == "  helo wrld  ");

    std::atomic<bool> unknown_done{false};
    manager.ExecuteTransform("Does Not Exist", "abc", [&](const std::string&) {
        unknown_done.store(true);
    });
    for (int i = 0; i < 100 && !unknown_done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(unknown_done.load());
}

TEST_CASE("transform manager tolerates a missing transforms file") {
    ScriptedLlm llm;
    TransformManager manager(llm, "tests/data/definitely_missing.json");
    CHECK_FALSE(manager.HasTransform("Fix Grammar"));
}

TEST_CASE("transform manager executes tasks strictly in order on one worker") {
    ScriptedLlm llm;
    TransformManager manager(llm);

    std::vector<int> order;
    std::mutex order_mutex;
    std::atomic<int> completed{0};
    for (int i = 0; i < 64; ++i) {
        manager.Post([&order, &order_mutex, &completed, i] {
            {
                std::lock_guard<std::mutex> lock(order_mutex);
                order.push_back(i);
            }
            completed.fetch_add(1);
        });
    }
    while (completed.load() < 64) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(order.size() == 64);
    for (int i = 0; i < 64; ++i) {
        REQUIRE(order[static_cast<std::size_t>(i)] == i);
    }
}

TEST_CASE("transform manager process async resolves with the provider result") {
    ScriptedLlm llm;
    llm.response = "cleaned text";
    TransformManager manager(llm);

    std::future<std::string> future = manager.ProcessAsync("dirty text", "prompt");
    REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    CHECK(future.get() == "cleaned text");
}

TEST_CASE("transform manager process async survives a provider exception") {
    ScriptedLlm llm;
    llm.cancelled_earlier = true;
    llm.cancel_count.store(1);
    TransformManager manager(llm);

    std::future<std::string> future = manager.ProcessAsync("text", "prompt");
    REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    CHECK(future.get().empty());
}
