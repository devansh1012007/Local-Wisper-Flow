#include "Fakes.h"

#include "core/CommandModeController.h"
#include "core/DictationCleanup.h"
#include "core/DictationEngine.h"
#include "core/TextPipeline.h"

#include <doctest/doctest.h>
#include <optional>
#include <string>

namespace {

using namespace lwf_test;

class FakeSelectionCapture final : public ISelectionCapture {
public:
    std::optional<std::string> selection = std::string("the selected sentence goes here");
    int restore_count = 0;
    int capture_count = 0;

    std::optional<std::string> CaptureSelection() override {
        ++capture_count;
        return selection;
    }

    void RestoreClipboard() override { ++restore_count; }
};

struct CommandHarness {
    FakeClock clock;
    FakeAudioCapture capture;
    ScriptedVad vad{false};
    FakeStt stt;
    PassthroughLlm pipeline_llm;
    DictationCleanup cleanup{&pipeline_llm};
    TextPipeline pipeline{cleanup};
    RecordingInjector injector;
    lwf::DictationFSM fsm;
    RecordingSink sink;
    lwf::EngineSettings settings;
    FakeSelectionCapture selection;
    ScriptedLlm command_llm;
    CommandModeController controller{selection, &command_llm, injector};

    std::unique_ptr<lwf::DictationEngine> MakeEngine() {
        auto engine = std::make_unique<lwf::DictationEngine>(capture, &vad, stt, pipeline,
                                                             injector, fsm, clock, sink, settings);
        engine->SetCommandController(&controller);
        return engine;
    }

    void Speak(int chunks = 4) {
        vad.SetSpeech(true);
        const std::vector<float> chunk(512, 0.3f);
        for (int i = 0; i < chunks; ++i) {
            clock.Advance(30);
            capture.Append(chunk);
        }
    }
};

}

TEST_CASE("command mode edits the selection end to end") {
    CommandHarness h;
    h.command_llm.reply = "the edited sentence";
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    CHECK(h.fsm.GetCurrentState() == State::ARMED);
    CHECK(h.selection.capture_count == 1);

    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    REQUIRE(h.injector.injected.size() == 1);
    CHECK(h.injector.injected[0] == "the edited sentence");
    CHECK(h.selection.restore_count >= 1);
    CHECK(h.command_llm.last_text == "the selected sentence goes here");
    CHECK(h.command_llm.last_prompt.find("You edit text") != std::string::npos);
    CHECK(h.command_llm.last_prompt.find("hello world") != std::string::npos);
    CHECK(engine->LastText() == "the edited sentence");
}

TEST_CASE("command mode without a selection reports and restores") {
    CommandHarness h;
    h.selection.selection = std::nullopt;
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(h.selection.restore_count == 1);
    CHECK(h.capture.start_count == 0);
    CHECK(h.command_llm.call_count == 0);
    bool reported = false;
    for (const std::string& status : h.sink.statuses) {
        if (status.find("select some text first") != std::string::npos) {
            reported = true;
        }
    }
    CHECK(reported);
}

TEST_CASE("command mode without the language model emits ModelMissing") {
    CommandHarness h;
    h.controller = CommandModeController(h.selection, nullptr, h.injector);
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::ModelMissing);
    CHECK(h.injector.injected.empty());
}

TEST_CASE("command mode llm failure leaves selection untouched and restores clipboard") {
    CommandHarness h;
    h.command_llm.fail = true;
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(h.injector.injected.empty());
    CHECK(h.selection.restore_count >= 1);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::LlmFailed);
}

TEST_CASE("command mode rejects preamble answers") {
    CommandHarness h;
    h.command_llm.reply = "Sure, here is the edited text";
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(h.injector.injected.empty());
    CHECK(h.selection.restore_count >= 1);
}

TEST_CASE("command mode injector failure restores clipboard and reports") {
    CommandHarness h;
    h.command_llm.reply = "edited output";
    h.injector.fail = true;
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(h.selection.restore_count >= 1);
    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::InjectFailed);
}

TEST_CASE("cancel during command instruction restores the clipboard") {
    CommandHarness h;
    auto engine = h.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h.Speak();
    engine->PostEvent(EngineEventType::Cancel);
    engine->ProcessOne(0);

    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(h.selection.restore_count == 1);
    CHECK(h.command_llm.call_count == 0);
    CHECK(h.injector.injected.empty());
}

TEST_CASE("command mode times out cooperatively") {
    class SlowLlm final : public ILlmProvider {
    public:
    std::atomic<bool> cancel_requested{false};
        std::string Process(const std::string&, const std::string&) override {
            const auto start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count() < 500) {
                if (cancel_requested.load()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return "late";
        }
        void RequestCancel() override { cancel_requested.store(true); }
    } slow_llm;

    CommandHarness h2;
    h2.controller = CommandModeController(h2.selection, &slow_llm, h2.injector);
    auto engine = h2.MakeEngine();

    engine->PostEvent(EngineEventType::CommandMode);
    engine->ProcessOne(0);
    h2.Speak();
    engine->PostEvent(EngineEventType::PttRelease);
    engine->ProcessOne(0);

    CHECK(h2.fsm.GetCurrentState() == State::IDLE);
    CHECK(h2.injector.injected.empty());
    CHECK(h2.selection.restore_count >= 1);
    REQUIRE(h2.sink.errors.size() == 1);
    CHECK(h2.sink.errors[0] == ErrorCode::LlmTimeout);
}
