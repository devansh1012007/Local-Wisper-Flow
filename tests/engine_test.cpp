#include <doctest/doctest.h>

#include "core/DictationCleanup.h"
#include "core/DictationEngine.h"
#include "core/EngineEvent.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "fakes.h"

namespace {

struct EngineHarness {
    FakeAudioCapture audio;
    FakeStt stt;
    RecordingInjector injector;
    DictationFSM fsm;
    ScriptedLlm llm;
    std::unique_ptr<DictationCleanup> cleanup;
    std::unique_ptr<DictationEngine> engine;
    EngineEventQueue queue;
    std::atomic<bool> quit{false};
    std::thread thread;
    EngineStatusSink sink;

    explicit EngineHarness(EngineSettings settings = EngineSettings{},
                           CleanupSettings cleanup_settings = CleanupSettings{}) {
        settings.min_hold = settings.min_hold.count() == 100 ? std::chrono::milliseconds(0)
                                                            : settings.min_hold;
        cleanup = std::make_unique<DictationCleanup>(llm, cleanup_settings);
        engine = std::make_unique<DictationEngine>(audio, stt, *cleanup, injector, fsm, nullptr,
                                                   settings, &sink);
        audio.SetChunkSink(engine->Sink());
        thread = std::thread([this] { engine->Run(queue, quit); });
    }

    ~EngineHarness() {
        quit.store(true);
        queue.Close();
        if (thread.joinable()) {
            thread.join();
        }
    }

    void Press(std::chrono::steady_clock::time_point at = std::chrono::steady_clock::now()) {
        queue.Push(EngineEvent{EngineEventType::PttPress, at});
    }

    void Release(std::chrono::steady_clock::time_point at = std::chrono::steady_clock::now()) {
        queue.Push(EngineEvent{EngineEventType::PttRelease, at});
    }

    void Cancel() {
        queue.Push(EngineEvent{EngineEventType::Cancel, std::chrono::steady_clock::now()});
    }

    bool WaitCapturing(std::chrono::milliseconds timeout = std::chrono::milliseconds(4000)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (fsm.CurrentState() == State::CAPTURING && audio.started.load()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return fsm.CurrentState() == State::CAPTURING && audio.started.load();
    }

    bool WaitError(EngineErrorCode code,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds(4000)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (sink.LastError() == code) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return sink.LastError() == code;
    }

    bool WaitIdle(std::chrono::milliseconds timeout = std::chrono::milliseconds(4000)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (fsm.CurrentState() == State::IDLE) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return fsm.CurrentState() == State::IDLE;
    }

    bool WaitInjected(std::chrono::milliseconds timeout = std::chrono::milliseconds(4000)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (injector.HasInjected()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return injector.HasInjected();
    }
};

}  // namespace

TEST_CASE("press release dictates and injects") {
    EngineHarness harness;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
    CHECK(harness.injector.LastInjected() == "hello world from the test");
    CHECK(harness.stt.transcribed_sizes.size() == 1);
}

TEST_CASE("tap shorter than min hold is ignored") {
    EngineSettings settings;
    settings.min_hold = std::chrono::milliseconds(500);
    EngineHarness harness(settings);
    const auto press_at = std::chrono::steady_clock::now();
    harness.Press(press_at);
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release(press_at + std::chrono::milliseconds(100));
    REQUIRE(harness.WaitIdle());
    CHECK_FALSE(harness.injector.HasInjected());
    CHECK(harness.stt.transcribed_sizes.empty());
}

TEST_CASE("silence only recording is discarded") {
    EngineHarness harness;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSilence(16000));
    harness.Release();
    REQUIRE(harness.WaitIdle());
    CHECK_FALSE(harness.injector.HasInjected());
    CHECK(harness.stt.transcribed_sizes.empty());
}

TEST_CASE("stt failure returns the fsm to idle and emits an error") {
    EngineHarness harness;
    harness.stt.fail_transcribe = true;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitIdle());
    CHECK_FALSE(harness.injector.HasInjected());
    CHECK(harness.sink.LastError() == EngineErrorCode::SttFailed);
}

TEST_CASE("injector failure returns the fsm to idle and emits an error") {
    EngineHarness harness;
    harness.injector.fail_inject = true;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitIdle());
    CHECK(harness.sink.LastError() == EngineErrorCode::InjectFailed);
}

TEST_CASE("mic start failure emits an error and the engine stays alive") {
    EngineHarness harness;
    harness.audio.fail_start = true;
    harness.Press();
    REQUIRE(harness.WaitError(EngineErrorCode::MicUnavailable));
    REQUIRE(harness.WaitIdle());
    CHECK_FALSE(harness.injector.HasInjected());

    harness.audio.fail_start = false;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
}

TEST_CASE("mic stop failure emits mic lost and returns to idle") {
    EngineHarness harness;
    harness.audio.fail_stop = true;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitIdle());
    CHECK(harness.sink.LastError() == EngineErrorCode::MicLost);
    CHECK_FALSE(harness.injector.HasInjected());
}

TEST_CASE("press while transcribing is dropped not queued for later") {
    EngineHarness harness;
    harness.stt.delay_ms = 250;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(harness.fsm.CurrentState() == State::TRANSCRIBING);
    harness.Press();
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK(harness.stt.transcribed_sizes.size() == 1);
}

TEST_CASE("cancel during capture injects nothing") {
    EngineHarness harness;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Cancel();
    REQUIRE(harness.WaitIdle());
    CHECK_FALSE(harness.injector.HasInjected());
    CHECK(harness.stt.transcribed_sizes.empty());
}

TEST_CASE("hands free silence auto stop transcribes") {
    EngineSettings settings;
    settings.hands_free_silence_ms = 400;
    EngineHarness harness(settings);
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.audio.FeedSamples(MakeSilence(16000));
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
}

TEST_CASE("max recording duration forces a stop") {
    EngineSettings settings;
    settings.max_recording = std::chrono::milliseconds(1000);
    EngineHarness harness(settings);
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000 * 2));
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
}

TEST_CASE("cleanup timeout falls back to raw transcript") {
    CleanupSettings cleanup_settings;
    cleanup_settings.timeout = std::chrono::milliseconds(50);
    EngineHarness harness(EngineSettings{}, cleanup_settings);
    harness.llm.delay_ms = 300;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
    CHECK(harness.injector.LastInjected() == "hello world from the test");
    CHECK(harness.llm.cancel_count.load() == 1);
}

TEST_CASE("device lost mid capture reports mic lost and returns to idle") {
    EngineHarness harness;
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.audio.device_lost.store(true);
    REQUIRE(harness.WaitError(EngineErrorCode::MicLost));
    REQUIRE(harness.WaitIdle());
    CHECK_FALSE(harness.injector.HasInjected());
    CHECK(harness.stt.transcribed_sizes.empty());

    harness.audio.device_lost.store(false);
    harness.Press();
    REQUIRE(harness.WaitCapturing());
    harness.audio.FeedSamples(MakeSpeech(16000));
    harness.Release();
    REQUIRE(harness.WaitInjected());
    REQUIRE(harness.WaitIdle());
}

TEST_CASE("two hundred consecutive dictations leave the engine responsive") {
    EngineHarness harness;
    for (int i = 0; i < 200; ++i) {
        harness.Press();
        REQUIRE(harness.WaitCapturing());
        harness.audio.FeedSamples(MakeSpeech(4000));
        harness.Release();
        REQUIRE(harness.WaitIdle());
        REQUIRE(harness.injector.HasInjected());
        if (i == 0) {
            REQUIRE(harness.injector.LastInjected() == "hello world from the test");
        }
    }
    CHECK(harness.stt.transcribed_sizes.size() == 200);
}
