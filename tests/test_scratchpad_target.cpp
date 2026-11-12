#include "EngineHarness.h"

#include "core/CommandModeController.h"
#include "core/NotesStore.h"
#include "core/SettingsStore.h"
#include "core/StatsStore.h"
#include "core/UiBus.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

std::string TestDir(const char* name) {
    const std::string dir =
        (std::filesystem::temp_directory_path() / (std::string("lwf_scratch_") + name)).string();
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string ReadSource(const std::string& path) {
    const std::filesystem::path full = std::filesystem::path(LWF_SOURCE_DIR) / path;
    std::ifstream file(full.string());
    if (!file.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

class CountingSelectionCapture final : public ISelectionCapture {
public:
    std::optional<std::string> CaptureSelection() override {
        ++capture_count;
        return std::nullopt;
    }

    void RestoreClipboard() override {
        ++restore_count;
    }

    int capture_count = 0;
    int restore_count = 0;
};

}

TEST_CASE("scratchpad buffer appends lines and clears") {
    ScratchpadBuffer buffer;
    CHECK(buffer.Text().empty());
    buffer.Append("hello world");
    CHECK(buffer.Text() == "hello world");
    buffer.Append("second line");
    CHECK(buffer.Text() == "hello world\nsecond line");
    buffer.Append("");
    CHECK(buffer.Text() == "hello world\nsecond line");
    std::string copy = buffer.Text();
    copy.append(" mutated");
    CHECK(buffer.Text() == "hello world\nsecond line");
    buffer.Clear();
    CHECK(buffer.Text().empty());
    buffer.Append("fresh");
    CHECK(buffer.Text() == "fresh");
    buffer.Append("trail\n");
    CHECK(buffer.Text() == "fresh\ntrail\n");
    buffer.Append("next");
    CHECK(buffer.Text() == "fresh\ntrail\nnext");
}

TEST_CASE("scratchpad injector writes only to the buffer") {
    ScratchpadBuffer buffer;
    ScratchpadInjector injector(buffer);
    RecordingInjector elsewhere;
    injector.InjectText("dictated line");
    CHECK(buffer.Text() == "dictated line");
    CHECK(elsewhere.InjectionCount() == 0);
    injector.InjectText("next line");
    CHECK(buffer.Text() == "dictated line\nnext line");
    CHECK(elsewhere.InjectionCount() == 0);
    CHECK(elsewhere.LastInjected().empty());
}

TEST_CASE("scratchpad target receives engine dictation nowhere else") {
    Harness h;
    CountingSelectionCapture capture_spy;
    ScratchpadBuffer buffer;
    ScratchpadInjector injector(buffer);
    auto engine = std::make_unique<DictationEngine>(h.capture, &h.vad, h.stt, h.pipeline, injector,
                                                    h.fsm, h.clock, h.sink, h.settings);
    engine->SetSelectionCapture(&capture_spy);
    PressAndSpeak(h, *engine);
    CHECK(h.injector.InjectionCount() == 0);
    CHECK(buffer.Text() == "hello world");
    CHECK(capture_spy.capture_count == 0);
    CHECK(capture_spy.restore_count == 0);
    CHECK(h.fsm.GetCurrentState() == State::IDLE);
    CHECK(engine->LastText() == "hello world");
    CHECK(HasStatus(h.sink.statuses, "injected"));
    h.stt.transcript = "second line";
    PressAndSpeak(h, *engine);
    CHECK(h.injector.InjectionCount() == 0);
    CHECK(buffer.Text() == "hello world\nsecond line");
    CHECK(capture_spy.capture_count == 0);
    CHECK(capture_spy.restore_count == 0);
}

TEST_CASE("scratchpad target suppresses stats record through engine flag") {
    const std::string dir = TestDir("statsflag");
    const std::string path = (std::filesystem::path(dir) / "stats.json").string();
    std::filesystem::remove(path);
    Harness h;
    ScratchpadBuffer buffer;
    ScratchpadInjector injector(buffer);
    StatsStore stats(path, true);
    auto engine = std::make_unique<DictationEngine>(h.capture, &h.vad, h.stt, h.pipeline, injector,
                                                    h.fsm, h.clock, h.sink, h.settings);
    engine->SetStatsStore(&stats);
    engine->SetScratchpadTarget(true);
    PressAndSpeak(h, *engine);
    CHECK(h.injector.InjectionCount() == 0);
    CHECK(buffer.Text() == "hello world");
    CHECK(stats.Generation() == 0);
    CHECK(stats.Snapshot().empty());
}

TEST_CASE("scratchpad flag off still records stats") {
    const std::string dir = TestDir("statsflagoff");
    const std::string path = (std::filesystem::path(dir) / "stats.json").string();
    std::filesystem::remove(path);
    Harness h;
    ScratchpadBuffer buffer;
    ScratchpadInjector injector(buffer);
    StatsStore stats(path, true);
    auto engine = std::make_unique<DictationEngine>(h.capture, &h.vad, h.stt, h.pipeline, injector,
                                                    h.fsm, h.clock, h.sink, h.settings);
    engine->SetStatsStore(&stats);
    engine->SetScratchpadTarget(false);
    PressAndSpeak(h, *engine);
    REQUIRE(h.injector.InjectionCount() == 0);
    CHECK(buffer.Text() == "hello world");
    CHECK(stats.Generation() == 1);
}

TEST_CASE("scratchpad ui command is delivered through the bus") {
    UiBus bus;
    bus.PostCommand(UiCommandType::OpenScratchpad, "");
    UiCommand command;
    REQUIRE(bus.PollCommand(command));
    CHECK(command.type == UiCommandType::OpenScratchpad);
    CHECK_FALSE(bus.PollCommand(command));
}

TEST_CASE("scratchpad overlay button wakes the badge and routes the command") {
    const std::string overlay_h = ReadSource("src/ui/Overlay.h");
    REQUIRE_FALSE(overlay_h.empty());
    CHECK(overlay_h.find("SetScratchpadVisible") != std::string::npos);
    CHECK(overlay_h.find("SetScratchpadActivateHandler") != std::string::npos);
    CHECK(overlay_h.find("scratchpad_visible_") != std::string::npos);
    CHECK(overlay_h.find("poll_note_hover_") != std::string::npos);
    const std::string overlay_cpp = ReadSource("src/ui/Overlay.cpp");
    REQUIRE_FALSE(overlay_cpp.empty());
    CHECK(overlay_cpp.find("SetScratchpadVisible") != std::string::npos);
    CHECK(overlay_cpp.find("SetScratchpadActivateHandler") != std::string::npos);
    CHECK(overlay_cpp.find("InvisibleButton(\"##note\"") != std::string::npos);
    CHECK(overlay_cpp.find("on_scratchpad_activate_") != std::string::npos);
    CHECK(overlay_cpp.find("poll_note_hover") != std::string::npos);
    CHECK(overlay_cpp.find("poll_note_hover_ = poll_note_hover") != std::string::npos);
    CHECK(overlay_cpp.find("dirty_ = true") != std::string::npos);
    const std::string main_cpp = ReadSource("src/main.cpp");
    REQUIRE_FALSE(main_cpp.empty());
    CHECK(main_cpp.find("OpenScratchpad") != std::string::npos);
    CHECK(main_cpp.find("SetScratchpadVisible") != std::string::npos);
    CHECK(main_cpp.find("SetScratchpadActivateHandler") != std::string::npos);
    CHECK(main_cpp.find("scratchpad_flow_bar") != std::string::npos);
    const std::string uibus_h = ReadSource("src/core/UiBus.h");
    REQUIRE_FALSE(uibus_h.empty());
    CHECK(uibus_h.find("OpenScratchpad") != std::string::npos);
}

// Source pin (followups-2): right-clicking the note icon removes it from the
// mini window without a trip to the Scratchpad page. The removal flips
// scratchpad_flow_bar off, saves, and re-applies through the same drain that
// the page toggle uses, so the window re-hugs to the dot-only extent.
TEST_CASE("mini window note icon is removable with a right click") {
    const std::string overlay_h = ReadSource("src/ui/Overlay.h");
    REQUIRE_FALSE(overlay_h.empty());
    CHECK(overlay_h.find("SetScratchpadRemoveHandler") != std::string::npos);
    CHECK(overlay_h.find("on_scratchpad_remove_") != std::string::npos);

    const std::string overlay_cpp = ReadSource("src/ui/Overlay.cpp");
    REQUIRE_FALSE(overlay_cpp.empty());
    CHECK(overlay_cpp.find("SetScratchpadRemoveHandler") != std::string::npos);
    CHECK(overlay_cpp.find("ImGui::IsItemClicked(1) && on_scratchpad_remove_") != std::string::npos);

    const std::string main_cpp = ReadSource("src/main.cpp");
    REQUIRE_FALSE(main_cpp.empty());
    const std::size_t wiring = main_cpp.find("SetScratchpadRemoveHandler");
    REQUIRE(wiring != std::string::npos);
    const std::string wiring_span = main_cpp.substr(wiring, 400);
    CHECK(wiring_span.find("SetBool(\"scratchpad_flow_bar\", false)") != std::string::npos);
    CHECK(wiring_span.find("config.Save()") != std::string::npos);
    CHECK(wiring_span.find("UiCommandType::ApplySettings") != std::string::npos);

    // The Scratchpad page surfaces the gesture next to the flow-bar toggle.
    const std::string page_cpp = ReadSource("src/ui/pages/ScratchpadPage.cpp");
    REQUIRE_FALSE(page_cpp.empty());
    CHECK(page_cpp.find("Right-click it to remove") != std::string::npos);
}

TEST_CASE("scratchpad flow bar key defaults off and persists") {
    const std::string dir = TestDir("cfg");
    const std::string cfg = (std::filesystem::path(dir) / "config.json").string();
    {
        SettingsStore store(cfg);
        CHECK(store.HasKey("scratchpad_flow_bar"));
        CHECK_FALSE(store.GetBool("scratchpad_flow_bar"));
        CHECK(store.IsHotApply("scratchpad_flow_bar"));
        store.SetBool("scratchpad_flow_bar", true);
        store.Save();
    }
    {
        SettingsStore reloaded(cfg);
        CHECK(reloaded.GetBool("scratchpad_flow_bar"));
        CHECK(reloaded.HasKey("keep_usage_stats"));
        CHECK(reloaded.GetBool("keep_usage_stats"));
    }
}
