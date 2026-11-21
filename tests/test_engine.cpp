#include "Fakes.h"

#include "EngineHarness.h"

#include <doctest/doctest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace lwf_test;

#include "EngineHarness.h"

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

TEST_CASE("press and release dictates and injects cleaned text") {
    Harness h;
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    REQUIRE(h.injector.injected.size() == 1);
    CHECK(h.injector.injected[0] == "hello world");
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(engine->LastText() == "hello world");
    CHECK(h.stt.transcribe_count == 1);
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

    CHECK(h.injector.injected.empty());
    CHECK(h.stt.transcribe_count == 0);
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

    CHECK(h.injector.injected.empty());
    CHECK(h.stt.transcribe_count == 0);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(HasStatus(h.sink.statuses, "discarded"));
}

TEST_CASE("stt failure returns fsm to idle and emits an error event") {
    Harness h;
    h.stt.fail_transcribe = true;
    auto engine = MakeEngine(h);
    PressAndSpeak(h, *engine);

    CHECK(h.injector.injected.empty());
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::SttFailed);
}

TEST_CASE("injector failure returns fsm to idle and emits an error event") {
    Harness h;
    h.injector.fail = true;
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

    REQUIRE(h.injector.injected.size() == 1);
    CHECK(h.injector.injected[0] == "hello world");
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

    REQUIRE(h.injector.injected.size() == 1);
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

    CHECK(h.injector.injected.empty());
    CHECK(h.stt.transcribe_count == 0);
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

    CHECK(h.injector.injected.empty());
    CHECK(h.stt.transcribe_count == 1);
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
        REQUIRE(h.injector.injected.size() == static_cast<std::size_t>(i + 1));
        CHECK(h.capture.start_count == i + 1);
    }

    const int threads_after = CountThreads();
    CHECK(threads_after == threads_before);
    CHECK(h.capture.buffer.size() == static_cast<std::size_t>(4 * kChunkSamples));
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

    REQUIRE(h.injector.injected.size() == 1);
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
    REQUIRE(h.injector.injected.size() == 1);
    const std::string first = h.injector.injected[0];

    engine->PostEvent(EngineEventType::PasteLast);
    engine->ProcessOne(0);
    REQUIRE(h.injector.injected.size() == 2);
    CHECK(h.injector.injected[1] == first);
    CHECK(HasStatus(h.sink.statuses, "pasted last transcript"));
}

TEST_CASE("paste last with empty history reports nothing to paste") {
    Harness h;
    auto engine = MakeEngine(h);

    engine->PostEvent(EngineEventType::PasteLast);
    engine->ProcessOne(0);
    CHECK(h.injector.injected.empty());
    CHECK(HasStatus(h.sink.statuses, "nothing to paste"));
}
