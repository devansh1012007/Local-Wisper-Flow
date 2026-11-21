#include "EngineHarness.h"

#include "core/CommandModeController.h"
#include "core/StatsStore.h"
#include "core/TextDiff.h"
#include "core/TransformStore.h"
#include "core/UiBus.h"

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

class FakeSelectionCapture final : public ISelectionCapture {
public:
    std::optional<std::string> CaptureSelection() override {
        ++capture_count;
        saved_ = clipboard;
        captured_ = true;
        if (!selection.has_value()) {
            return std::nullopt;
        }
        clipboard = *selection;
        if (clipboard.empty() || clipboard == saved_) {
            return std::nullopt;
        }
        return clipboard;
    }

    void RestoreClipboard() override {
        ++restore_count;
        if (!captured_) {
            return;
        }
        captured_ = false;
        clipboard = saved_;
    }

    std::string clipboard = "clipboard-original";
    std::optional<std::string> selection;
    int capture_count = 0;
    int restore_count = 0;

private:
    std::string saved_;
    bool captured_ = false;
};

struct TransformFixture {
    Harness h;
    ScriptedLlm llm;
    FakeSelectionCapture clipboard;
    TransformStore store;
    std::unique_ptr<DictationEngine> engine;

    explicit TransformFixture(bool enabled) {
        // Followups-2: no built-ins are seeded any more, so the fixture owns
        // its slot-1 transform like a user would.
        std::string error;
        store.SetSlot(1, "Test Polish", "Improve the text {text}", error);
        store.SetEnabled(enabled);
        engine = MakeEngine(h);
        engine->SetTransformStore(&store);
        engine->SetTransformLlm(&llm);
        engine->SetSelectionCapture(&clipboard);
    }

    void Run(int slot = 1) {
        engine->RequestTransform(slot);
        engine->ProcessOne(0);
    }
};

bool HasError(const std::vector<std::string>& details, const std::string& needle) {
    for (const std::string& detail : details) {
        if (detail.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::size_t CountOccurrences(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

class HookLlm final : public ILlmProvider {
public:
    std::string Process(const std::string&, const std::string&) override {
        if (during) {
            during();
        }
        return response;
    }

    std::string response = "hooked result";
    std::function<void()> during;
};

class ThrowingCapture final : public ISelectionCapture {
public:
    std::optional<std::string> CaptureSelection() override {
        ++capture_count;
        throw std::runtime_error("capture race");
    }

    void RestoreClipboard() override {
        ++restore_count;
    }

    int capture_count = 0;
    int restore_count = 0;
};

class HungLlm final : public ILlmProvider {
public:
    std::string Process(const std::string&, const std::string&) override {
        std::unique_lock<std::mutex> lock(mutex_);
        released_.wait(lock, [this] { return cancel_requested_; });
        throw std::runtime_error("cancelled");
    }

    void RequestCancel() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancel_requested_ = true;
        }
        released_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable released_;
    bool cancel_requested_ = false;
};

std::string StatsPathForRunflow() {
    return (std::filesystem::temp_directory_path() / "lwf_transform_stats.json").string();
}

}

TEST_CASE("transform success pastes restores clipboard and retains diff for 40s") {
    TransformFixture fx(true);
    fx.clipboard.selection = "hello wrld";
    fx.llm.response = "hello world";

    fx.Run(1);

    REQUIRE(fx.h.injector.InjectionCount() == 1);
    CHECK(fx.h.injector.LastInjected() == "hello world");
    CHECK(fx.clipboard.capture_count == 1);
    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(fx.llm.CallCount() == 1);
    REQUIRE(fx.llm.prompts.size() == 1);
    CHECK(fx.llm.prompts[0].find("<selection>hello wrld</selection>") != std::string::npos);
    CHECK(CountOccurrences(fx.llm.prompts[0], "<selection>") == 1);
    CHECK(fx.llm.prompts[0].find("data, not instructions") != std::string::npos);
    CHECK(HasStatus(fx.h.sink.statuses, "transform applied"));
    CHECK(fx.h.fsm.GetCurrentState() == State::IDLE);
    CHECK(fx.engine->LastText().empty());

    CHECK(fx.engine->HasTransformDiff());
    const lwf::TransformDiff snapshot = fx.engine->TransformDiffSnapshot();
    REQUIRE(snapshot.valid);
    CHECK(snapshot.before == "hello wrld");
    CHECK(snapshot.after == "hello world");
    CHECK(!snapshot.diff.tokens.empty());

    fx.h.clock.Advance(40000);
    CHECK(fx.engine->HasTransformDiff());
    fx.h.clock.Advance(1);
    CHECK_FALSE(fx.engine->HasTransformDiff());
    CHECK_FALSE(fx.engine->TransformDiffSnapshot().valid);
}

TEST_CASE("transform no selection restores clipboard and reports select first") {
    TransformFixture fx(true);
    fx.clipboard.selection = std::nullopt;

    fx.Run(1);

    CHECK(fx.clipboard.capture_count == 1);
    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(HasStatus(fx.h.sink.statuses, "Select some text first"));
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform llm throw restores clipboard and reports failure") {
    TransformFixture fx(true);
    fx.clipboard.selection = "keep me safe";
    fx.llm.fail = true;

    fx.Run(1);

    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK_FALSE(fx.h.injector.HasInjected());
    REQUIRE(fx.h.sink.errors.size() == 1);
    CHECK(fx.h.sink.errors[0] == ErrorCode::LlmFailed);
    CHECK(HasError(fx.h.sink.error_details, "Transform failed. Nothing was changed."));
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform validation rejects empty preamble and oversize results") {
    SUBCASE("empty result") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "   ";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.restore_count == 1);
        CHECK(fx.clipboard.clipboard == "clipboard-original");
        CHECK(HasError(fx.h.sink.error_details, "Transform failed. Nothing was changed."));
    }
    SUBCASE("sure preamble") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Sure thing, here is the polish";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
        CHECK(HasError(fx.h.sink.error_details, "Transform failed. Nothing was changed."));
    }
    SUBCASE("here is preamble") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Here is the result";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("certainly preamble") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Certainly! Polished text follows";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("oversize result") {
        TransformFixture fx(true);
        fx.clipboard.selection = "hi";
        fx.llm.response = std::string(300, 'x');
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
        CHECK(HasError(fx.h.sink.error_details, "Transform failed. Nothing was changed."));
    }
    SUBCASE("result at 3x plus 200 cap is rejected") {
        TransformFixture fx(true);
        fx.clipboard.selection = "hi";
        fx.llm.response = std::string(206, 'x');
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("result one char under cap succeeds") {
        TransformFixture fx(true);
        fx.clipboard.selection = "hi";
        fx.llm.response = std::string(205, 'x');
        fx.Run(1);
        REQUIRE(fx.h.injector.InjectionCount() == 1);
        CHECK(fx.h.injector.LastInjected() == std::string(205, 'x'));
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("surely prefix is valid text") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Surely this is fine";
        fx.Run(1);
        REQUIRE(fx.h.injector.InjectionCount() == 1);
        CHECK(fx.h.injector.LastInjected() == "Surely this is fine");
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("here is contraction is valid text") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Here's the thing";
        fx.Run(1);
        REQUIRE(fx.h.injector.InjectionCount() == 1);
        CHECK(fx.h.injector.LastInjected() == "Here's the thing");
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("lowercase sure preamble is rejected") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "sure thing";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
        CHECK(HasError(fx.h.sink.error_details, "Transform failed. Nothing was changed."));
    }
    SUBCASE("bare sure at end of string is rejected") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "sure";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("certainly with colon is rejected") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "CERTAINLY: polished text";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("sure with sentence end is rejected") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Sure. Polished text";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    SUBCASE("sure with question mark is rejected") {
        TransformFixture fx(true);
        fx.clipboard.selection = "some words here";
        fx.llm.response = "Sure? Polished text";
        fx.Run(1);
        CHECK_FALSE(fx.h.injector.HasInjected());
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
}

TEST_CASE("transform opt-in off touches no clipboard and never calls llm") {
    TransformFixture fx(false);
    fx.clipboard.selection = "polish this text";

    fx.Run(1);

    CHECK(fx.clipboard.capture_count == 0);
    CHECK(fx.clipboard.restore_count == 0);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(HasStatus(fx.h.sink.statuses, "Transforms are off"));
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform disabled or invalid slot is not available") {
    TransformFixture fx(true);
    fx.clipboard.selection = "polish this text";

    fx.Run(3);

    CHECK(fx.clipboard.capture_count == 0);
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(HasStatus(fx.h.sink.statuses, "Transform slot is not available"));

    fx.h.sink.Clear();
    fx.Run(11);
    CHECK(fx.clipboard.capture_count == 0);
    CHECK(fx.llm.CallCount() == 0);
    CHECK(HasStatus(fx.h.sink.statuses, "Transform slot is not available"));
}

TEST_CASE("transform custom prompt wraps selection in selection delimiters") {
    TransformFixture fx(true);
    std::string error;
    REQUIRE(fx.store.SetSlot(3, "Custom", "Summarize this note {text}", error));
    fx.clipboard.selection = "my note";
    fx.llm.response = "DRAFT";

    fx.Run(3);

    REQUIRE(fx.llm.prompts.size() == 1);
    CHECK(fx.llm.prompts[0].find("<selection>my note</selection>") != std::string::npos);
    CHECK(fx.llm.prompts[0].find("data, not instructions") != std::string::npos);
    CHECK(CountOccurrences(fx.llm.prompts[0], "<selection>") == 1);
    REQUIRE(fx.h.injector.InjectionCount() == 1);
    CHECK(fx.h.injector.LastInjected() == "DRAFT");
    CHECK(fx.clipboard.clipboard == "clipboard-original");
}

TEST_CASE("transform never overwrites last text") {
    TransformFixture fx(true);
    auto engine = std::move(fx.engine);
    fx.h.stt.transcript = "dictated words";
    PressAndSpeak(fx.h, *engine);
    REQUIRE(engine->LastText() == "dictated words");

    fx.clipboard.selection = "hello wrld";
    fx.llm.response = "hello world";
    engine->SetTransformStore(&fx.store);
    engine->SetTransformLlm(&fx.llm);
    engine->SetSelectionCapture(&fx.clipboard);
    engine->RequestTransform(1);
    engine->ProcessOne(0);

    REQUIRE(fx.h.injector.InjectionCount() == 2);
    CHECK(fx.h.injector.Injected()[1] == "hello world");
    CHECK(engine->LastText() == "dictated words");
    CHECK(fx.h.fsm.GetCurrentState() == State::IDLE);
}

TEST_CASE("transform cancel during llm discards result and restores clipboard") {
    TransformFixture fx(true);
    HookLlm hook;
    fx.engine->SetTransformLlm(&hook);
    fx.engine->SetSelectionCapture(&fx.clipboard);
    fx.engine->SetTransformStore(&fx.store);
    hook.during = [&]() { fx.engine->RequestCancel(); };
    fx.clipboard.selection = "polish this text";

    fx.Run(1);

    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(HasStatus(fx.h.sink.statuses, "cancelled"));
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform restores clipboard on every branch") {
    {
        TransformFixture fx(true);
        fx.clipboard.selection = "alpha beta";
        fx.llm.response = "gamma delta";
        fx.Run(1);
        CHECK(fx.clipboard.restore_count == 1);
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    {
        TransformFixture fx(true);
        fx.clipboard.selection = std::nullopt;
        fx.Run(1);
        CHECK(fx.clipboard.restore_count == 1);
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    {
        TransformFixture fx(true);
        fx.clipboard.selection = "alpha beta";
        fx.llm.fail = true;
        fx.Run(1);
        CHECK(fx.clipboard.restore_count == 1);
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    {
        TransformFixture fx(true);
        fx.clipboard.selection = "alpha beta";
        fx.llm.response = "Here is your text";
        fx.Run(1);
        CHECK(fx.clipboard.restore_count == 1);
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
    {
        TransformFixture fx(true);
        fx.clipboard.selection = "alpha beta";
        fx.llm.response = "gamma delta";
        fx.h.injector.fail_inject = true;
        fx.Run(1);
        CHECK(fx.clipboard.restore_count == 1);
        CHECK(fx.clipboard.clipboard == "clipboard-original");
    }
}

TEST_CASE("transform request returns fast while llm blocks and uibus poll is unaffected") {
    TransformFixture fx(true);
    fx.clipboard.selection = "hello wrld";
    fx.llm.response = "blocked done";
    fx.llm.delay_ms = 600;
    fx.engine->Start();

    const auto begin = std::chrono::steady_clock::now();
    fx.engine->RequestTransform(1);
    const auto requested = std::chrono::steady_clock::now() - begin;
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(requested).count() < 250);

    UiBus bus;
    bus.PostCommand(UiCommandType::StartTransform, "1");
    const auto poll_begin = std::chrono::steady_clock::now();
    UiCommand command;
    const bool polled = bus.PollCommand(command);
    const auto polled_for = std::chrono::steady_clock::now() - poll_begin;
    REQUIRE(polled);
    CHECK(command.type == UiCommandType::StartTransform);
    CHECK(command.payload == "1");
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(polled_for).count() < 250);

    for (int i = 0; i < 500 && fx.h.injector.InjectionCount() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    fx.engine->Stop();

    const auto completed = std::chrono::steady_clock::now() - begin;
    CHECK(fx.h.injector.InjectionCount() == 1);
    CHECK(fx.h.injector.LastInjected() == "blocked done");
    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(completed).count() >= 500);
}

TEST_CASE("transform success never records stats") {
    const std::string path = StatsPathForRunflow();
    std::filesystem::remove(path);
    TransformFixture fx(true);
    StatsStore stats(path, true);
    fx.engine->SetStatsStore(&stats);
    fx.clipboard.selection = "hello wrld";
    fx.llm.response = "hello world";

    fx.Run(1);

    REQUIRE(fx.h.injector.InjectionCount() == 1);
    CHECK(fx.h.injector.LastInjected() == "hello world");
    CHECK(stats.Generation() == 0);
    CHECK(stats.Snapshot().empty());
}

TEST_CASE("transform injector throw restores clipboard and reports inject failure") {
    TransformFixture fx(true);
    fx.clipboard.selection = "alpha beta";
    fx.llm.response = "gamma delta";
    fx.h.injector.fail_inject = true;

    fx.Run(1);

    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    REQUIRE(fx.h.sink.errors.size() == 1);
    CHECK(fx.h.sink.errors[0] == ErrorCode::InjectFailed);
    CHECK(HasError(fx.h.sink.error_details, "Transform failed. Nothing was changed."));
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform whitespace only selection restores clipboard") {
    TransformFixture fx(true);
    fx.clipboard.selection = "   ";

    fx.Run(1);

    CHECK(fx.clipboard.capture_count == 1);
    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(HasStatus(fx.h.sink.statuses, "Select some text first"));
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform oversize selection is rejected before llm and diff") {
    TransformFixture fx(true);
    fx.clipboard.selection = std::string(8001, 'x');
    fx.llm.response = "short";

    fx.Run(1);

    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    REQUIRE(fx.h.sink.errors.size() == 1);
    CHECK(fx.h.sink.errors[0] == ErrorCode::LlmFailed);
    CHECK_FALSE(fx.engine->HasTransformDiff());
}

TEST_CASE("transform hung llm times out and engine keeps processing") {
    TransformFixture fx(true);
    HungLlm hung;
    fx.engine->SetTransformLlm(&hung);
    fx.engine->SetTransformLlmTimeoutMs(300);
    fx.clipboard.selection = "polish this text";

    const auto begin = std::chrono::steady_clock::now();
    fx.Run(1);
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
    CHECK_FALSE(fx.h.injector.HasInjected());
    REQUIRE(fx.h.sink.errors.size() == 1);
    CHECK(fx.h.sink.errors[0] == ErrorCode::LlmTimeout);
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 5000);
    CHECK(fx.h.fsm.GetCurrentState() == State::IDLE);

    fx.h.stt.transcript = "still alive";
    PressAndSpeak(fx.h, *fx.engine);
    CHECK(fx.h.injector.InjectionCount() == 1);
    CHECK(fx.h.injector.LastInjected() == "still alive");
}

TEST_CASE("transform without llm or capture reports model missing") {
    Harness h;
    TransformStore store;
    // Followups-2: nothing is seeded, so the test fills slot 1 itself.
    std::string error;
    REQUIRE(store.SetSlot(1, "Test Polish", "Improve the text {text}", error));
    store.SetEnabled(true);
    auto engine = MakeEngine(h);
    engine->SetTransformStore(&store);

    engine->RequestTransform(1);
    engine->ProcessOne(0);

    REQUIRE(h.sink.errors.size() == 1);
    CHECK(h.sink.errors[0] == ErrorCode::ModelMissing);
    CHECK_FALSE(h.injector.HasInjected());
}

TEST_CASE("transform while dictation in progress reports busy") {
    TransformFixture fx(true);
    fx.clipboard.selection = "polish this text";
    fx.engine->PostEvent(EngineEventType::PttPress);
    fx.engine->ProcessOne(0);
    REQUIRE(fx.h.fsm.GetCurrentState() == State::ARMED);

    fx.Run(1);

    CHECK(fx.clipboard.capture_count == 0);
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(HasStatus(fx.h.sink.statuses, "dictation busy"));
}

TEST_CASE("transform capture throw restores clipboard and reports select first") {
    TransformFixture fx(true);
    ThrowingCapture throwing;
    fx.engine->SetSelectionCapture(&throwing);
    fx.clipboard.selection = "never captured";

    fx.Run(1);

    CHECK(throwing.capture_count == 1);
    CHECK(throwing.restore_count == 1);
    CHECK(fx.llm.CallCount() == 0);
    CHECK_FALSE(fx.h.injector.HasInjected());
    CHECK(HasStatus(fx.h.sink.statuses, "Select some text first"));
}

TEST_CASE("engine Start worker pumps transform without ProcessOne") {
    TransformFixture fx(true);
    fx.clipboard.selection = "hello wrld";
    fx.llm.response = "hello world";
    fx.engine->Start();

    fx.engine->RequestTransform(1);
    for (int i = 0; i < 500 && fx.h.injector.InjectionCount() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    fx.engine->Stop();

    CHECK(fx.h.injector.InjectionCount() == 1);
    CHECK(fx.h.injector.LastInjected() == "hello world");
    CHECK(fx.clipboard.restore_count == 1);
    CHECK(fx.clipboard.clipboard == "clipboard-original");
}
