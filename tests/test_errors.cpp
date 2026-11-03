#include "Fakes.h"

#include "EngineHarness.h"

#include "core/DictationCleanup.h"
#include "core/DictationEngine.h"
#include "core/RateLimiter.h"
#include "core/TextPipeline.h"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace lwf_test;

class UnavailableMic final : public IAudioCapture {
public:
    void Start() override { throw std::runtime_error("no mic device"); }
    void Stop() override {}
    std::vector<float> GetRecordedAudio() override { return {}; }
};

}

TEST_CASE("each failure kind maps to exactly one error code and idle fsm") {
    SUBCASE("stt failure") {
        Harness h;
        h.stt.fail_transcribe = true;
        auto engine = MakeEngine(h);
        PressAndSpeak(h, *engine);
        REQUIRE(h.sink.errors.size() == 1);
        CHECK(h.sink.errors[0] == ErrorCode::SttFailed);
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
    }
    SUBCASE("inject failure") {
        Harness h;
        h.injector.fail = true;
        auto engine = MakeEngine(h);
        PressAndSpeak(h, *engine);
        REQUIRE(h.sink.errors.size() == 1);
        CHECK(h.sink.errors[0] == ErrorCode::InjectFailed);
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
    }
    SUBCASE("mic unavailable") {
        Harness h;
        UnavailableMic mic;
        auto engine = std::make_unique<DictationEngine>(mic, &h.vad, h.stt, h.pipeline,
                                                             h.injector, h.fsm, h.clock, h.sink,
                                                             h.settings);
        engine->PostEvent(EngineEventType::PttPress);
        engine->ProcessOne(0);
        REQUIRE(h.sink.errors.size() == 1);
        CHECK(h.sink.errors[0] == ErrorCode::MicUnavailable);
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
    }
    SUBCASE("cancel emits no error") {
        Harness h;
        auto engine = MakeEngine(h);
        engine->PostEvent(EngineEventType::PttPress);
        engine->ProcessOne(0);
        h.Speak();
        engine->PostEvent(EngineEventType::Cancel);
        engine->ProcessOne(0);
        CHECK(h.sink.errors.empty());
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
    }
    SUBCASE("llm cleanup failure falls back to raw without error event") {
        Harness h;
        ScriptedLlm failing_llm;
        failing_llm.fail = true;
        DictationCleanup cleanup(&failing_llm);
        TextPipeline pipeline(cleanup);
        auto engine = std::make_unique<DictationEngine>(h.capture, &h.vad, h.stt, pipeline,
                                                             h.injector, h.fsm, h.clock, h.sink,
                                                             h.settings);
        PressAndSpeak(h, *engine);
        CHECK(h.sink.errors.empty());
        REQUIRE(h.injector.injected.size() == 1);
        CHECK(h.injector.injected[0] == "hello world");
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
    }
}

TEST_CASE("rate limiter allows one per window per code") {
    RateLimiter limiter(10000);
    CHECK(limiter.Allow(ErrorCode::MicLost, 1000));
    CHECK_FALSE(limiter.Allow(ErrorCode::MicLost, 5000));
    CHECK_FALSE(limiter.Allow(ErrorCode::MicLost, 10999));
    CHECK(limiter.Allow(ErrorCode::MicLost, 11001));
    CHECK(limiter.Allow(ErrorCode::SttFailed, 11001));
}

namespace {

std::vector<std::string> ListSourceFiles(const std::string& directory) {
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec)) {
        if (entry.is_directory()) {
            continue;
        }
        const std::string path = entry.path().string();
        if (path.size() > 4 &&
            (path.rfind(".cpp") == path.size() - 4 || path.rfind(".h") == path.size() - 2)) {
            files.push_back(path);
        }
    }
    return files;
}

}

TEST_CASE("no empty catch blocks anywhere in src") {
    const std::string source_dir = LWF_SOURCE_DIR;
    const std::vector<std::string> files = ListSourceFiles(source_dir + "/src");
    CHECK(files.size() > 20);

    const std::regex empty_catch(R"(catch\s*\([^)]*\)\s*\{\s*\})");
    for (const std::string& path : files) {
        std::ifstream file(path);
        CHECK(file.is_open());
        std::ostringstream stream;
        stream << file.rdbuf();
        const std::string content = stream.str();
        INFO("file=", path);
        CHECK_FALSE(std::regex_search(content, empty_catch));
    }
}
