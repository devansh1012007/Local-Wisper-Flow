#include "Fakes.h"

#include "core/TransformManager.h"

#include <doctest/doctest.h>
#include <atomic>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace lwf_test;

void WriteTransformFile(const char* path) {
    std::ofstream file(path);
    file << R"([
        {"name": "Fix Grammar", "prompt": "Fix: {text}"}
    ])";
}

#ifdef __linux__
int CountThreads() {
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("Threads:", 0) == 0) {
            std::istringstream stream(line.substr(8));
            int count = 0;
            stream >> count;
            return count;
        }
    }
    return -1;
}
#endif

}

TEST_CASE("one thousand transform calls run on a single worker") {
#ifdef __linux__
    WriteTransformFile("transforms_test_tmp.json");
    PassthroughLlm llm;
    const int threads_before = CountThreads();
    REQUIRE(threads_before > 0);

    std::atomic<int> completed{0};
    {
        TransformManager manager(llm, "transforms_test_tmp.json");
        for (int i = 0; i < 1000; ++i) {
            manager.ExecuteTransform("Fix Grammar", "  word   " + std::to_string(i) + "  ",
                                     [&](std::string) { completed.fetch_add(1); });
        }
        for (int i = 0; i < 2000 && completed.load() < 1000; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK(completed.load() == 1000);
        CHECK(CountThreads() == threads_before + 1);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(CountThreads() == threads_before);
#else
    CHECK(true);
#endif
}
