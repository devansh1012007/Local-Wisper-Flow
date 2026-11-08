#include "Fakes.h"

#include "EngineHarness.h"

#include "core/AppContextResolver.h"
#include "core/CommandModeController.h"
#include "core/PersonalDictionary.h"
#include "core/StatsStore.h"

#include <doctest/doctest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

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

std::string StatsPath(const char* name) {
    return (std::filesystem::temp_directory_path() /
            (std::string("lwf_stats_wiring_") + name + ".json"))
        .string();
}

std::int64_t StatsTotalWords(const StatsStore& store) {
    std::int64_t total = 0;
    for (const auto& entry : store.Snapshot()) {
        total += entry.second.words;
    }
    return total;
}

std::int64_t StatsTotalDictations(const StatsStore& store) {
    std::int64_t total = 0;
    for (const auto& entry : store.Snapshot()) {
        total += entry.second.dictations;
    }
    return total;
}

class ScriptedActiveWindow final : public IActiveWindow {
public:
    ActiveWindowInfo info;

    ActiveWindowInfo Get() override {
        return info;
    }
};

class ScriptedSelectionCapture final : public ISelectionCapture {
public:
    std::optional<std::string> selection = std::string("the selected sentence");

    std::optional<std::string> CaptureSelection() override {
        return selection;
    }

    void RestoreClipboard() override {}
};

}

TEST_CASE("press and release dictates and injects cleaned text") {
    Harness h;
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello world");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(engine->LastText() == "hello world");
    CHECK(h.stt.TranscribeCount() == 1);
    CHECK(HasStatus(h.sink.statuses, "injected"));
}

TEST_CASE("tap shorter than the minimum hold is ignored") {
    Harness h;
    h.settings.min_hold_ms = 250;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.clock.Advance(50);
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(h.stt.TranscribeCount() == 0);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("silence-only recording is discarded") {
    Harness h;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.clock.Advance(300);
    h.Silence();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(h.stt.TranscribeCount() == 0);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "discarded"));
}

TEST_CASE("stt failure returns fsm to idle and emits an error event") {
    Harness h;
    h.stt.fail_transcribe = true;
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::SttFailed);
}

TEST_CASE("injector failure returns fsm to idle and emits an error event") {
    Harness h;
    h.injector.fail_inject = true;
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::InjectFailed);
}

TEST_CASE("hands-free silence auto-stop finishes the dictation") {
    Harness h;
    h.settings.hands_free_silence_ms = 200;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::Toggle);
    engine->ProcessOne(0);
    h.Speak();
    h.clock.Advance(250);
    engine->ProcessOne(0);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello world");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("max recording timeout stops and transcribes") {
    Harness h;
    h.settings.max_recording_ms = 1000;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.Speak();
    h.clock.Advance(1500);
    engine->ProcessOne(0);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("cancel during capturing discards audio and injects nothing") {
    Harness h;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::Cancel);
    engine->ProcessOne(0);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(h.stt.TranscribeCount() == 0);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("cancel during transcribing prevents injection even if stt returns later") {
    Harness h;
    auto engine = MakeEngine(h);
    h.stt.on_transcribe = [&]() { engine->RequestCancel(); };

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(h.stt.TranscribeCount() == 1);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "cancelled"));
}

TEST_CASE("two hundred consecutive dictations leave thread count and buffers flat") {
#ifdef __linux__
    Harness h;
    auto engine = MakeEngine(h);

    const int threads_before = CountThreads();
    REQUIRE(threads_before > 0);

    for (int i = 0; i < 200; ++i) {
        engine->PostEvent(EngineEventType::PttPress);
        engine->ProcessOne(0);
        h.Speak(4);
        engine->PostEvent(EngineEventType::PttRelease);
        engine->ProcessOne(0);
        REQUIRE(h.injector.InjectionCount() == static_cast<std::size_t>(i + 1));
        CHECK(h.capture.start_count.load() == static_cast<std::size_t>(i + 1));
    }

    const int threads_after = CountThreads();
    CHECK(threads_after == threads_before);
    CHECK(h.capture.GetRecordedAudio().size() == static_cast<std::size_t>(4 * kChunkSamples));
#else
    CHECK(true);
#endif
}

TEST_CASE("double tap ptt activates hands-free mode through the tracker") {
    Harness h;
    h.settings.hands_free_silence_ms = 200;
    auto engine = MakeEngine(h);

    HotkeyTracker tracker;
    auto ptt = HotkeyBinding::Parse("Ctrl+Win", BindingRole::PushToTalk);
    REQUIRE(ptt.has_value());
    tracker.SetBindings({*ptt});

    RawKeyEvent down{HotkeyBinding::kCtrl, true, false, 1'000'000};
    RawKeyEvent win_down{HotkeyBinding::kWin, true, false, 1'000'010};
    RawKeyEvent win_up{HotkeyBinding::kWin, false, false, 1'000'050};
    RawKeyEvent ctrl_up{HotkeyBinding::kCtrl, false, false, 1'000'060};

    for (const RawKeyEvent* event : {&down, &win_down, &win_up, &ctrl_up}) {
        const TrackerResult result = tracker.Feed(*event);
        for (const EngineEvent& engine_event : result.events) {
            engine->PostEvent(engine_event.type);
        }
    }
    engine->ProcessOne(0);
    engine->ProcessOne(0);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);

    std::int64_t base = 1'000'100;
    RawKeyEvent down2{HotkeyBinding::kCtrl, true, false, base};
    RawKeyEvent win_down2{HotkeyBinding::kWin, true, false, base + 10};
    for (const RawKeyEvent* event : {&down2, &win_down2}) {
        const TrackerResult result = tracker.Feed(*event);
        for (const EngineEvent& engine_event : result.events) {
            engine->PostEvent(engine_event.type);
        }
    }
    engine->ProcessOne(0);
    CHECK(h.fsm.GetCurrentState() == State::ARMED);

    h.Speak();
    CHECK(h.fsm.GetCurrentState() == State::CAPTURING);
    h.clock.Advance(250);
    engine->ProcessOne(0);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("cancel fires the fsm cancel trigger from capturing and transcribing") {
    SUBCASE("capturing") {
        Harness h;
        auto engine = MakeEngine(h);
        engine->PostEvent(EngineEventType::PttPress);
        engine->ProcessOne(0);
        h.Speak();
        engine->PostEvent(EngineEventType::Cancel);
        engine->ProcessOne(0);
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
        CHECK(HasStatus(h.sink.statuses, "cancelled"));
    }
    SUBCASE("transcribing") {
        Harness h;
        auto engine = MakeEngine(h);
        h.stt.on_transcribe = [&] { engine->RequestCancel(); };
        engine->PostEvent(EngineEventType::PttPress);
        engine->ProcessOne(0);
        h.Speak();
        engine->PostEvent(EngineEventType::PttRelease);
        engine->ProcessOne(0);
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
        CHECK(HasStatus(h.sink.statuses, "cancelled"));
    }
}

TEST_CASE("paste last re-injects the last cleaned text") {
    Harness h;
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);
    REQUIRE(h.injector.InjectionCount() == 1);
    const std::string first = h.injector.Injected()[0];

    engine->PostEvent(EngineEventType::PasteLast);
    engine->ProcessOne(0);
    REQUIRE(h.injector.InjectionCount() == 2);
    CHECK(h.injector.Injected()[1] == first);
    CHECK(HasStatus(h.sink.statuses, "pasted last transcript"));
}

TEST_CASE("paste last with empty history reports nothing to paste") {
    Harness h;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PasteLast);
    engine->ProcessOne(0);
    CHECK_FALSE(h.injector.HasInjected());
    CHECK(HasStatus(h.sink.statuses, "nothing to paste"));
}

TEST_CASE("stats store records successful injection") {
    const std::string path = StatsPath("success");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(store.Generation() == 1);
    CHECK(StatsTotalDictations(store) == 1);
    CHECK(StatsTotalWords(store) == 2);
    const auto snap = store.Snapshot();
    REQUIRE(snap.size() == 1);
    CHECK(snap.begin()->second.words_corrected == 0);
    CHECK(snap.begin()->second.dictionary_fixes == 0);
    CHECK(snap.begin()->second.speaking_ms == 120);
    REQUIRE(snap.begin()->second.apps.find("unknown") != snap.begin()->second.apps.end());
    CHECK(snap.begin()->second.apps.at("unknown") == 2);
    std::filesystem::remove(path);
}

TEST_CASE("stats record uses capture time app and duration") {
    const std::string path = StatsPath("captureapp");
    std::filesystem::remove(path);
    StatsStore store(path);
    AppContextResolver resolver;
    resolver.UseBuiltInProfiles();
    ScriptedActiveWindow active;
    active.info.process_name = "slack.exe";
    active.info.window_title = "Team chat";
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);
    engine->SetActiveWindow(&active);
    engine->SetAppContextResolver(&resolver);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.Speak();
    active.info.process_name = "outlook.exe";
    active.info.window_title = "Inbox";
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(store.Generation() == 1);
    const auto snap = store.Snapshot();
    REQUIRE(snap.size() == 1);
    CHECK(snap.begin()->second.speaking_ms == 120);
    REQUIRE(snap.begin()->second.apps.find("slack.exe") !=
            snap.begin()->second.apps.end());
    CHECK(snap.begin()->second.apps.at("slack.exe") == 2);
    CHECK(snap.begin()->second.apps.find("outlook.exe") ==
          snap.begin()->second.apps.end());
    REQUIRE(snap.begin()->second.by_category.find("Work messages") !=
            snap.begin()->second.by_category.end());
    CHECK(snap.begin()->second.by_category.at("Work messages") == 2);
    std::filesystem::remove(path);
}

TEST_CASE("stats record carries words corrected and dictionary fixes") {
    const std::string path = StatsPath("fixes");
    std::filesystem::remove(path);
    StatsStore store(path);
    PersonalDictionary dict;
    dict.SetWords({"hello", "world"});
    dict.SetReplacements({{"helo", "hello"}});
    Harness h;
    h.stt.transcript = "helo world";
    h.pipeline.SetDictionary(&dict);
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello world");
    CHECK(store.Generation() == 1);
    const auto snap = store.Snapshot();
    REQUIRE(snap.size() == 1);
    CHECK(snap.begin()->second.words == 2);
    CHECK(snap.begin()->second.words_corrected == 1);
    CHECK(snap.begin()->second.dictionary_fixes == 1);
    std::filesystem::remove(path);
}

TEST_CASE("pipeline process result counts diff and dictionary fixes") {
    PassthroughLlm llm;
    DictationCleanup cleanup{&llm};
    cleanup.SetEnabled(false);
    TextPipeline pipeline{cleanup};
    PersonalDictionary dict;
    dict.SetWords({"hello", "world"});
    dict.SetReplacements({{"helo", "hello"}});
    pipeline.SetDictionary(&dict);

    const ProcessResult result = pipeline.ProcessWithStats("helo world");
    CHECK(result.text == "hello world");
    CHECK(result.words_corrected == 1);
    CHECK(result.dictionary_fixes == 1);
    CHECK(pipeline.Process("helo world") == result.text);

    PassthroughLlm plain_llm;
    DictationCleanup plain_cleanup{&plain_llm};
    plain_cleanup.SetEnabled(false);
    TextPipeline plain{plain_cleanup};
    const ProcessResult identical = plain.ProcessWithStats("hello world");
    CHECK(identical.text == "hello world");
    CHECK(identical.words_corrected == 0);
    CHECK(identical.dictionary_fixes == 0);
}

TEST_CASE("pipeline words corrected counts inserted words from cleanup") {
    ScriptedLlm llm;
    llm.response = "hello brave world ok now";
    DictationCleanup cleanup{&llm};
    cleanup.SetMinWords(1);
    TextPipeline pipeline{cleanup};

    const ProcessResult result = pipeline.ProcessWithStats("hello world ok now");
    CHECK(result.text == "hello brave world ok now");
    CHECK(result.words_corrected == 1);
    CHECK(result.dictionary_fixes == 0);
}

TEST_CASE("stats store records nothing for cancelled capture") {
    const std::string path = StatsPath("cancelled");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::Cancel);
    engine->ProcessOne(0);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats store records nothing for cancel during transcription") {
    const std::string path = StatsPath("canceltranscribe");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);
    h.stt.on_transcribe = [&]() { engine->RequestCancel(); };

    PressAndSpeak(h, *engine);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats store records nothing for empty final text") {
    const std::string path = StatsPath("emptyfinal");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    h.stt.transcript = "";
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats store records nothing for failed injection") {
    const std::string path = StatsPath("failedinject");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    h.injector.fail_inject = true;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::InjectFailed);
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats store records nothing for command mode") {
    const std::string path = StatsPath("commandmode");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);
    ScriptedSelectionCapture selection;
    ScriptedLlm command_llm;
    command_llm.response = "the edited sentence";
    CommandModeController controller{selection, &command_llm, h.injector};
    engine->SetCommandController(&controller);

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "the edited sentence");
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats store records nothing for paste last") {
    const std::string path = StatsPath("pastelast");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);
    REQUIRE(h.injector.InjectionCount() == 1);
    REQUIRE(store.Generation() == 1);

    engine->PostEvent(EngineEventType::PasteLast);
    engine->ProcessOne(0);

    REQUIRE(h.injector.InjectionCount() == 2);
    CHECK(store.Generation() == 1);
    CHECK(StatsTotalDictations(store) == 1);
    std::filesystem::remove(path);
}

TEST_CASE("stats store records nothing for scratchpad target") {
    const std::string path = StatsPath("scratchpad");
    std::filesystem::remove(path);
    StatsStore store(path);
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);
    engine->SetScratchpadTarget(true);

    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats store toggle off records nothing") {
    const std::string path = StatsPath("toggleoff");
    std::filesystem::remove(path);
    StatsStore store(path, false);
    CHECK_FALSE(store.UsageStatsEnabled());
    Harness h;
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("engine works with no stats store attached") {
    Harness h;
    auto engine = MakeEngine(h);

    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello world");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("stats whitespace only final text records nothing") {
    const std::string path = StatsPath("whitespacefinal");
    std::filesystem::remove(path);
    StatsStore store(path, true);
    Harness h;
    h.stt.transcript = "   ";
    auto engine = MakeEngine(h);
    engine->SetStatsStore(&store);

    PressAndSpeak(h, *engine);

    CHECK_FALSE(h.injector.HasInjected());
    CHECK(store.Generation() == 0);
    CHECK(store.Snapshot().empty());
    std::filesystem::remove(path);
}

TEST_CASE("stats negative duration clamps to zero and still records") {
    const std::string path = StatsPath("negativeduration");
    std::filesystem::remove(path);
    StatsStore store(path, true);
    DictationRecord record;
    record.timestamp_local = 0;
    record.duration_ms = -100;
    record.final_text = "hello world";
    record.raw_text = "hello world";

    store.Record(record);

    REQUIRE(store.Generation() == 1);
    REQUIRE(store.Snapshot().size() == 1);
    CHECK(store.Snapshot().begin()->second.speaking_ms == 0);
    std::filesystem::remove(path);
}

TEST_CASE("engine settings hot apply changes armed timeout") {
    Harness h;
    auto engine = MakeEngine(h);
    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    REQUIRE(h.fsm.GetCurrentState() == State::ARMED);

    EngineSettings updated = h.settings;
    updated.max_recording_ms = 0;
    engine->SetSettings(updated);
    engine->Tick();

    CHECK(HasStatus(h.sink.statuses, "no speech detected, stopped"));
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}


TEST_CASE("blank audio marker yields nothing to inject") {
    SUBCASE("upper bracket marker") {
        Harness h;
        h.stt.transcript = "[BLANK_AUDIO]";
        auto engine = MakeEngine(h);
        PressAndSpeak(h, *engine);
        CHECK(h.injector.InjectionCount() == 0);
        CHECK_FALSE(h.injector.HasInjected());
        CHECK(engine->LastText().empty());
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
        CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
    }
    SUBCASE("lower bracket marker") {
        Harness h;
        h.stt.transcript = "[blank_audio]";
        auto engine = MakeEngine(h);
        PressAndSpeak(h, *engine);
        CHECK(h.injector.InjectionCount() == 0);
        CHECK_FALSE(h.injector.HasInjected());
        CHECK(engine->LastText().empty());
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
        CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
    }
    SUBCASE("paren music marker") {
        Harness h;
        h.stt.transcript = "(music)";
        auto engine = MakeEngine(h);
        PressAndSpeak(h, *engine);
        CHECK(h.injector.InjectionCount() == 0);
        CHECK_FALSE(h.injector.HasInjected());
        CHECK(engine->LastText().empty());
        CHECK(h.fsm.GetCurrentState() == State::IDLE);
        CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
    }
}

TEST_CASE("streamed blank audio marker yields nothing to inject") {
    Harness h;
    h.stt.transcript = "[BLANK_AUDIO]";
    int calls = 0;
    h.stt.on_transcribe = [&] {
        if (++calls > 1) {
            h.stt.transcript = "";
        }
    };
    h.settings.streaming_stt = true;
    h.settings.streaming_min_segment_ms = 3000;
    h.settings.streaming_pause_ms = 700;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        h.capture.FeedSamples(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 26; ++i) {
        h.capture.FeedSamples(silent);
        engine->ProcessOne(0);
    }
    CHECK(engine->StreamedSegments() == 1);
    h.vad.SetSpeech(true);
    for (int i = 0; i < 20; ++i) {
        h.capture.FeedSamples(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    for (int i = 0; i < 4; ++i) {
        h.capture.FeedSamples(silent);
        engine->ProcessOne(0);
    }
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.injector.InjectionCount() == 0);
    CHECK_FALSE(h.injector.HasInjected());
    CHECK(engine->LastText().empty());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
    CHECK(h.stt.TranscribeCount() == 2);
}

TEST_CASE("command mode blank audio marker is rejected") {
    Harness h;
    h.stt.transcript = "[BLANK_AUDIO]";
    auto engine = MakeEngine(h);
    ScriptedSelectionCapture selection;
    ScriptedLlm command_llm;
    command_llm.response = "the edited sentence";
    CommandModeController controller{selection, &command_llm, h.injector};
    engine->SetCommandController(&controller);

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.injector.InjectionCount() == 0);
    CHECK_FALSE(h.injector.HasInjected());
    CHECK(command_llm.CallCount() == 0);
    CHECK(engine->LastText().empty());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(h.sink.errors.empty());
    CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
}

TEST_CASE("real speech passes through the hallucination filter") {
    Harness h;
    h.stt.transcript = "hello world";
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello world");
    CHECK(engine->LastText() == "hello world");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("short real speech under cleanup minimum bypasses the language model") {
    Harness h;
    ScriptedLlm llm;
    llm.response = "should not appear";
    DictationCleanup custom_cleanup{&llm};
    TextPipeline custom_pipeline{custom_cleanup};
    h.stt.transcript = "  hello  ";
    DictationEngine engine(h.capture, &h.vad, h.stt, custom_pipeline, h.injector, h.fsm,
                           h.clock, h.sink, h.settings);
    PressAndSpeak(h, engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello");
    CHECK(llm.CallCount() == 0);
    CHECK(engine.LastText() == "hello");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("double blank audio marker yields nothing to inject") {
    Harness h;
    h.stt.transcript = "[BLANK_AUDIO] [BLANK_AUDIO]";
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    CHECK(h.injector.InjectionCount() == 0);
    CHECK_FALSE(h.injector.HasInjected());
    CHECK(engine->LastText().empty());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
}

TEST_CASE("streamed double blank audio marker yields nothing to inject") {
    Harness h;
    h.stt.transcript = "[BLANK_AUDIO]";
    h.settings.streaming_stt = true;
    h.settings.streaming_min_segment_ms = 3000;
    h.settings.streaming_pause_ms = 700;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PttPress);
    engine->ProcessOne(0);
    h.vad.SetSpeech(true);
    const std::vector<float> voiced(kChunkSamples, 0.3f);
    for (int i = 0; i < 100; ++i) {
        h.capture.FeedSamples(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    const std::vector<float> silent(kChunkSamples, 0.0f);
    for (int i = 0; i < 26; ++i) {
        h.capture.FeedSamples(silent);
        engine->ProcessOne(0);
    }
    CHECK(engine->StreamedSegments() == 1);
    h.vad.SetSpeech(true);
    for (int i = 0; i < 20; ++i) {
        h.capture.FeedSamples(voiced);
        engine->ProcessOne(0);
    }
    h.vad.SetSpeech(false);
    for (int i = 0; i < 4; ++i) {
        h.capture.FeedSamples(silent);
        engine->ProcessOne(0);
    }
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.injector.InjectionCount() == 0);
    CHECK_FALSE(h.injector.HasInjected());
    CHECK(engine->LastText().empty());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "nothing to inject"));
    CHECK(h.stt.TranscribeCount() == 2);
}

TEST_CASE("speech with trailing blank audio marker injects cleaned text") {
    Harness h;
    h.stt.transcript = "hello [BLANK_AUDIO]";
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.InjectionCount() == 1);
    CHECK(h.injector.Injected()[0] == "hello");
    CHECK(engine->LastText() == "hello");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
}
