#include "Fakes.h"

#include "EngineHarness.h"
#include "core/HistoryStore.h"

#include <doctest/doctest.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using namespace lwf_test;

class ChaosStt final : public IInferenceProvider {
public:
    bool fail = false;
    int calls = 0;

    std::string Transcribe(const std::vector<float>&) override {
        ++calls;
        if (fail) {
            throw std::runtime_error("chaos stt failure");
        }
        return "chaos text";
    }

    std::string ProcessText(const std::string& text, const std::string&) override { return text; }
};

class LostCapture final : public FakeAudioCapture {
public:
    bool lost = false;
    bool DeviceLost() const override { return lost; }
};

TEST_CASE("mic lost during capturing emits miclost and returns to idle") {
    Harness h;
    LostCapture capture;
    DictationCleanup cleanup(&h.llm);
    TextPipeline pipeline(cleanup);
    RecordingInjector injector;
    DictationFSM fsm;
    RecordingSink sink;
    FakeClock clock;
    EngineSettings settings;

    DictationEngine engine(capture, &h.vad, h.stt, pipeline, injector, fsm, clock, sink, settings);
    engine.Start();
    engine.PostEvent(EngineEventType::PttPress);
    engine.ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 6; ++i) {
        capture.Append(voiced);
        engine.ProcessOne(0);
    }
    REQUIRE(fsm.GetCurrentState() == State::CAPTURING);
    capture.lost = true;
    engine.ProcessOne(0);

    CHECK(fsm.GetCurrentState() == State::IDLE);
    REQUIRE(sink.errors.size() == 1);
    CHECK(sink.errors[0] == ErrorCode::MicLost);
    CHECK(injector.injected.empty());
    CHECK(HasStatus(sink.statuses, "microphone lost"));
    engine.Stop();
}

TEST_CASE("history store survives unwritable paths without throwing") {
    const std::filesystem::path base =
        std::filesystem::temp_directory_path() / "lwf_hist_unwritable";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    const std::string dir_as_file = (base / "history.jsonl").string();
    std::filesystem::create_directories(dir_as_file);

    HistoryStore store(dir_as_file, true, false, 100, 90);
    HistoryEntry entry;
    entry.timestamp = 1000;
    entry.text = "unwritable";
    entry.duration_ms = 5;
    CHECK_NOTHROW(store.Append(entry));
    REQUIRE(store.Last().has_value());
    CHECK(store.Last()->text == "unwritable");
    CHECK_NOTHROW(store.Clear());
    CHECK_NOTHROW(store.Export((base / "out.jsonl").string()));
    std::filesystem::remove_all(base);
}

TEST_CASE("cleanup with null llm passes raw text through unchanged") {
    DictationCleanup cleanup(nullptr);
    cleanup.SetEnabled(true);
    cleanup.SetMinWords(1);
    const std::string raw = "this stays exactly as spoken";
    CHECK(cleanup.Process(raw) == raw);
}

TEST_CASE("one thousand dictations with random failures always return to idle") {
    Harness h;
    ChaosStt stt;
    DictationCleanup cleanup(&h.llm);
    TextPipeline pipeline(cleanup);
    RecordingInjector injector;
    DictationFSM fsm;
    RecordingSink sink;
    FakeClock clock;
    EngineSettings settings;

    DictationEngine engine(h.capture, &h.vad, stt, pipeline, injector, fsm, clock, sink, settings);
    engine.Start();

    unsigned seed = 12345;
    auto next = [&seed]() {
        seed = seed * 1103515245u + 12345u;
        return (seed >> 16) % 100;
    };

    int injected = 0;
    int errors = 0;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        stt.fail = next() < 15;
        injector.fail = next() < 15;
        const bool cancel_early = next() < 10;
        const bool tap = next() < 10;
        const bool silence_only = next() < 10;

        engine.PostEvent(EngineEventType::PttPress);
        engine.ProcessOne(0);
        if (silence_only) {
            engine.PostEvent(EngineEventType::PttRelease);
            engine.ProcessOne(0);
        } else if (tap) {
            h.vad.SetSpeech(true);
            const std::vector<float> voiced(kChunkSamples, 0.3f);
            h.capture.Append(voiced);
            engine.PostEvent(EngineEventType::PttRelease);
            engine.ProcessOne(0);
        } else {
            h.vad.SetSpeech(true);
            const std::vector<float> voiced(kChunkSamples, 0.3f);
            for (int i = 0; i < 6; ++i) {
                clock.Advance(30);
                h.capture.Append(voiced);
            }
            if (cancel_early) {
                engine.PostEvent(EngineEventType::Cancel);
                engine.ProcessOne(0);
            }
            engine.PostEvent(EngineEventType::PttRelease);
            engine.ProcessOne(0);
        }

        if (fsm.GetCurrentState() != State::IDLE) {
            FAIL("fsm stuck in state ", static_cast<int>(fsm.GetCurrentState()), " at iteration ",
                 iteration);
        }
        injected += static_cast<int>(injector.injected.size());
        errors += static_cast<int>(sink.errors.size());
        injector.injected.clear();
        sink.errors.clear();
        sink.statuses.clear();
        clock.Advance(1000);
    }
    engine.Stop();
    CHECK(injected > 0);
    CHECK(errors > 0);
}

}
