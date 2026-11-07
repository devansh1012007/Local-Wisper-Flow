#include "core/InsightsModel.h"
#include "core/Languages.h"
#include "core/OverlayModel.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "ui/MainWindow.h"
#include "ui/theme/StudioPalette.h"
#include "ui/theme/ThemePalette.h"

#include <algorithm>
#include <doctest/doctest.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("waveform ring keeps the last N samples in order") {
    OverlayModel::WaveformRing ring(64);
    for (int i = 0; i < 70; ++i) {
        ring.Push(static_cast<float>(i));
    }
    CHECK(ring.Size() == 64);
    const std::vector<float> snapshot = ring.Snapshot();
    REQUIRE(snapshot.size() == 64);
    CHECK(snapshot.front() == 6.0f);
    CHECK(snapshot.back() == 69.0f);
}

TEST_CASE("visual mapping covers every mini state including dwell times") {
    const auto hidden = OverlayModel::VisualFor(OverlayModel::PillState::Hidden, "");
    CHECK_FALSE(hidden.visible);
    CHECK(hidden.dwell_after_ms == 0);

    const auto ready = OverlayModel::VisualFor(OverlayModel::PillState::Ready, "");
    CHECK(ready.visible);
    CHECK(ready.dwell_after_ms == 0);

    const auto listening = OverlayModel::VisualFor(OverlayModel::PillState::Listening, "");
    CHECK(listening.visible);
    CHECK(listening.dwell_after_ms == 0);

    const auto thinking = OverlayModel::VisualFor(OverlayModel::PillState::Thinking, "");
    CHECK(thinking.visible);
    CHECK(thinking.dwell_after_ms == 0);

    const auto paused = OverlayModel::VisualFor(OverlayModel::PillState::Paused, "");
    CHECK(paused.visible);
    CHECK(paused.dwell_after_ms == 0);

    const auto done = OverlayModel::VisualFor(OverlayModel::PillState::Done, "");
    CHECK(done.visible);
    CHECK(done.dwell_after_ms == 600);

    const auto attention =
        OverlayModel::VisualFor(OverlayModel::PillState::Attention, "mic blocked");
    CHECK(attention.visible);
    CHECK(attention.message == "mic blocked");
    CHECK(attention.dwell_after_ms == 6000);
}

TEST_CASE("pill position is bottom center above the taskbar with dpi scaling") {
    const OverlayModel::Rect work_area{0, 0, 1920, 1040};
    const OverlayModel::Rect position =
        OverlayModel::PillPosition(work_area, 200, 40, 16, 1.0);
    CHECK(position.x == 1920 / 2 - 100);
    CHECK(position.y == 1040 - 40 - 16);
    CHECK(position.width == 200);

    const OverlayModel::Rect scaled =
        OverlayModel::PillPosition(work_area, 200, 40, 16, 1.5);
    CHECK(scaled.width == 300);
    CHECK(scaled.height == 60);
    CHECK(scaled.x == 1920 / 2 - 150);
    CHECK(scaled.y == 1040 - 60 - 24);

    const OverlayModel::Rect second_monitor{1920, 0, 2560, 1400};
    const OverlayModel::Rect moved =
        OverlayModel::PillPosition(second_monitor, 200, 40, 16, 1.0);
    CHECK(moved.x == 1920 + 2560 / 2 - 100);
    CHECK(moved.y == 1400 - 40 - 16);
}

TEST_CASE("clamped mini position keeps the badge inside the work area") {
    const OverlayModel::Rect work_area{0, 0, 1920, 1040};

    // A position inside the area passes through unchanged.
    const OverlayModel::Rect inside = OverlayModel::ClampPosition(work_area, 100, 200, 140, 70);
    CHECK(inside.x == 100);
    CHECK(inside.y == 200);

    // Negative and past-edge positions are pulled back so the badge stays
    // reachable after a drag on any monitor layout.
    const OverlayModel::Rect negative = OverlayModel::ClampPosition(work_area, -500, -8, 140, 70);
    CHECK(negative.x == 0);
    CHECK(negative.y == 0);

    const OverlayModel::Rect past = OverlayModel::ClampPosition(work_area, 3000, 1500, 140, 70);
    CHECK(past.x == 1920 - 140);
    CHECK(past.y == 1040 - 70);

    // A window wider than the work area pins to the work area origin.
    const OverlayModel::Rect tiny_area{100, 100, 80, 60};
    const OverlayModel::Rect huge = OverlayModel::ClampPosition(tiny_area, 0, 0, 140, 70);
    CHECK(huge.x == 100);
    CHECK(huge.y == 100);

    // A second monitor to the left with negative coordinates.
    const OverlayModel::Rect left_monitor{-2560, 0, 2560, 1400};
    const OverlayModel::Rect left = OverlayModel::ClampPosition(left_monitor, -3000, 500, 140, 70);
    CHECK(left.x == -2560);
    CHECK(left.y == 500);
}

TEST_CASE("settings diff classifies hot apply versus restart required") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_diff_settings.json").string();
    {
        std::ofstream file(path, std::ios::trunc);
        file << "{}";
    }
    SettingsStore before(path);
    SettingsStore after_store(path);
    after_store.SetInt("min_hold_ms", 400);
    after_store.SetString("whisper_model_path", "ggml-new.bin");
    after_store.SetBool("sound_enabled", false);

    const SettingsUiModel::SettingDiff diff = SettingsUiModel::ClassifyDiff(before, after_store);
    REQUIRE(diff.hot_keys.size() == 2);
    CHECK(diff.hot_keys[0] == "min_hold_ms");
    CHECK(diff.hot_keys[1] == "sound_enabled");
    REQUIRE(diff.restart_keys.size() == 1);
    CHECK(diff.restart_keys[0] == "whisper_model_path");
}

TEST_CASE("validators reject out of range and unsupported values") {
    CHECK(SettingsUiModel::ValidateInt("min_hold_ms", 400, 50, 5000).empty());
    CHECK_FALSE(SettingsUiModel::ValidateInt("min_hold_ms", 10, 50, 5000).empty());
    CHECK(SettingsUiModel::ValidateModelPath("models/ggml.bin").empty());
    CHECK_FALSE(SettingsUiModel::ValidateModelPath(std::string(600, 'x')).empty());
    CHECK(SettingsUiModel::ValidateLanguage("en").empty());
    CHECK(SettingsUiModel::ValidateLanguage("auto").empty());
    CHECK_FALSE(SettingsUiModel::ValidateLanguage("xx").empty());
}

TEST_CASE("dictionary editor adds removes and dedupes") {
    SettingsUiModel::DictionaryEditorModel editor;
    editor.AddWord("Kubernetes");
    editor.AddWord("Docker");
    editor.AddWord("Kubernetes");
    editor.AddReplacement("gihub", "GitHub");
    editor.AddReplacement("gihub", "GitHub");
    editor.Dedupe();
    CHECK(editor.words.size() == 2);
    CHECK(editor.replacements.size() == 1);
    editor.RemoveWord(0);
    CHECK(editor.words.size() == 1);
    CHECK(editor.words[0].value == "Docker");
    editor.AddWord("");
    CHECK(editor.words.size() == 1);
}

TEST_CASE("snippet editor validates and dedupes") {
    SettingsUiModel::SnippetEditorModel editor;
    editor.Add("sig", "John Doe");
    editor.Add("sig", "Other");
    editor.Add("", "bad");
    editor.Dedupe();
    CHECK(editor.rows.size() == 1);

    std::string error;
    CHECK(editor.Valid(error));
    editor.Add("big", std::string(4001, 'x'));
    CHECK_FALSE(editor.Valid(error));
    CHECK(error.find("4000") != std::string::npos);
}

TEST_CASE("theme palettes keep the mockup token values") {
    using lwf::ui::kDarkPalette;
    using lwf::ui::kLightPalette;
    CHECK(kDarkPalette.bg == 0x0E1714u);
    CHECK(kDarkPalette.surface == 0x14221Du);
    CHECK(kDarkPalette.border == 0x29392Fu);
    CHECK(kDarkPalette.text == 0xEDE6D6u);
    CHECK(kDarkPalette.muted == 0xA8B3AAu);
    CHECK(kDarkPalette.accent == 0xC9A96Eu);
    CHECK(kDarkPalette.err == 0xD98A8Fu);
    CHECK(kLightPalette.bg == 0xF6F1E4u);
    CHECK(kLightPalette.surface == 0xFFFDF7u);
    CHECK(kLightPalette.text == 0x1C2B26u);
    CHECK(kLightPalette.accent == 0x1F3A5Fu);
    CHECK(kLightPalette.brass == 0x7A5C25u);
}

TEST_CASE("theme palettes satisfy the contrast policy except the two hairline pairs") {
    using lwf::ui::ContrastRatio;
    for (const lwf::ui::Palette& palette : {lwf::ui::kDarkPalette, lwf::ui::kLightPalette}) {
        INFO((palette.bg == 0x0E1714u ? "dark" : "light"));
        // Body text: 4.5:1 on page background and card surface.
        CHECK(ContrastRatio(palette.text, palette.bg) >= 4.5);
        CHECK(ContrastRatio(palette.text, palette.surface) >= 4.5);
        // Helper text: 4.5:1.
        CHECK(ContrastRatio(palette.muted, palette.bg) >= 4.5);
        CHECK(ContrastRatio(palette.muted, palette.surface) >= 4.5);
        // Meaningful colors: 4.5:1 (the mockup passes them at body level).
        CHECK(ContrastRatio(palette.accent, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.ok, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.warn, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.err, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.brass, palette.surface) >= 4.5);
        // Text on accent fills (primary buttons) at 4.5:1.
        CHECK(ContrastRatio(palette.on_accent, palette.accent) >= 4.5);
        // The two decorative hairline pairs are the documented exemption:
        // they sit below 3:1 by design and never carry state on their own.
        CHECK(ContrastRatio(palette.border, palette.bg) < 3.0);
        CHECK(ContrastRatio(palette.border, palette.surface) < 3.0);
    }
}

TEST_CASE("contrast math matches the WCAG formula") {
    using lwf::ui::ContrastRatio;
    CHECK(std::abs(ContrastRatio(0xEDE6D6, 0x0E1714) - 14.67) < 0.02);
    CHECK(std::abs(ContrastRatio(0xEDE6D6, 0x14221D) - 13.23) < 0.02);
    CHECK(std::abs(ContrastRatio(0xC9A96E, 0x14221D) - 7.35) < 0.02);
    CHECK(std::abs(ContrastRatio(0x1C2B26, 0xF6F1E4) - 13.09) < 0.02);
}

TEST_CASE("design px font tokens follow the mockup type scale") {
    namespace metrics = lwf::ui::metrics;
    // Approved T-001 design mapping: mockup type scale s1=28 (page title),
    // s2=18 (wordmark), s3=15 (card title), s4=14 (body) / s5=13 (helper) ->
    // the design-px tokens below, with body and helper text sharing the
    // font_body_px token per the approved design decision.
    CHECK(metrics::font_wordmark_px == 18.0f);
    CHECK(metrics::font_page_title_px == 28.0f);
    CHECK(metrics::font_card_title_px == 15.0f);
    CHECK(metrics::font_body_px == 13.0f);
    CHECK(metrics::font_mini_text_px == 8.5f);
}

TEST_CASE("src ui font mechanism uses scoped design px tokens without obsolete scale api") {
    // Regression guard for T-001 (AC1/AC2): the obsolete ImGui::SetWindowFontScale
    // must not reappear anywhere under src/ui, and display typography must stay
    // wired through the scoped design-px PushFont mechanism. lwf_tests does not
    // link ImGui, so the mechanism is guarded at the source/token level here.
    // Fail-before for the ApplyImGuiStyle gate (leftover R-W1-003-001): the e0a3882
    // delta added the last-applied (ImGuiContext*, float) gate plus design-px
    // reassignment before ScaleAllSizes. The markers below are asserted here so
    // this test FAILS against the pre-delta ThemePalette.cpp (no gate, old
    // `if (scale != 1.0f)` skip at scale 1.0) and PASSES post-delta.
    // Verified fail-before by comparing each marker string against
    // `git show e0a3882^:src/ui/theme/ThemePalette.cpp` (absent) and the
    // post-delta content (present) — see W1-IMPR-A2-T-001 follow-up section.
    const std::vector<std::string> font_sources = {
        "src/ui/MainWindow.cpp",
        "src/ui/MainWindow.h",
        "src/ui/Overlay.cpp",
        "src/ui/Overlay.h",
        "src/ui/theme/ThemePalette.h",
        "src/ui/theme/ThemePalette.cpp",
        "src/ui/theme/ThemeStyle.h",
        "src/ui/theme/StudioPalette.h",
        "src/ui/theme/StudioPalette.cpp",
        "src/ui/widgets/Widgets.h",
        "src/ui/widgets/Buttons.cpp",
        "src/ui/widgets/Display.cpp",
        "src/ui/widgets/Overlays.h",
        "src/ui/widgets/Overlays.cpp",
        "src/ui/pages/DictionaryPage.h",
        "src/ui/pages/DictionaryPage.cpp",
        "src/ui/pages/InsightsPage.h",
        "src/ui/pages/InsightsPage.cpp",
        "src/ui/pages/TransformsPage.h",
        "src/ui/pages/TransformsPage.cpp",
        "src/ui/pages/ScratchpadPage.h",
        "src/ui/pages/ScratchpadPage.cpp",
        "src/ui/pages/SnippetsPage.h",
        "src/ui/pages/SnippetsPage.cpp",
        "src/ui/ScratchpadWindow.h",
        "src/ui/ScratchpadWindow.cpp",
    };
    for (const std::string& path : font_sources) {
        std::ifstream file(path);
        REQUIRE(static_cast<bool>(file));
        INFO(path);
        const std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        CHECK(content.find("SetWindowFontScale") == std::string::npos);
    }

    // The Full window pushes the design-px token family through scoped fonts.
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string main_content((std::istreambuf_iterator<char>(main_window)),
                                   std::istreambuf_iterator<char>());
    CHECK(main_content.find("PushFont(nullptr, metrics::font_") != std::string::npos);

    // Gate markers (R-W1-003-001): ApplyImGuiStyle gates on the last-applied
    // (ImGuiContext*, scale) pair so ScaleAllSizes (cumulative / lossy) runs
    // only on an actual change, at every scale including 1.0.
    std::ifstream palette_cpp("src/ui/theme/ThemePalette.cpp");
    REQUIRE(static_cast<bool>(palette_cpp));
    const std::string palette_content((std::istreambuf_iterator<char>(palette_cpp)),
                                      std::istreambuf_iterator<char>());
    CHECK(palette_content.find("g_last_applied_context") != std::string::npos);
    CHECK(palette_content.find("g_last_applied_scale") != std::string::npos);
    CHECK(palette_content.find("const bool applies =") != std::string::npos);
    CHECK(palette_content.find("if (applies)") != std::string::npos);
    CHECK(palette_content.find("g_last_applied_scale = scale;") != std::string::npos);
    CHECK(palette_content.find("style.ScaleAllSizes(scale);") != std::string::npos);
    // The pre-delta `if (scale != 1.0f)` guard skipped the apply when moving
    // back to 100% (metrics stuck scaled). It must not return.
    CHECK(palette_content.find("if (scale != 1.0f)") == std::string::npos);
    // Design-px geometry must be reassigned before the scale call so every
    // apply starts from the design base (this is what makes a re-apply
    // idempotent despite ScaleAllSizes being cumulative).
    CHECK(palette_content.find("style.WindowRounding = metrics::radius_card;") !=
          std::string::npos);
    CHECK(palette_content.find("style.WindowRounding = metrics::radius_card;") <
          palette_content.find("style.ScaleAllSizes(scale);"));

    // Header documents the once-per-change contract (absent pre-delta).
    std::ifstream palette_h("src/ui/theme/ThemePalette.h");
    REQUIRE(static_cast<bool>(palette_h));
    const std::string palette_header((std::istreambuf_iterator<char>(palette_h)),
                                     std::istreambuf_iterator<char>());
    CHECK(palette_header.find("once per actual change") != std::string::npos);
}

TEST_CASE("mini badge alphas meet the mockup contrast floors") {
    namespace metrics = lwf::ui::metrics;
    // GUI-A6: mockup section 2 note pins the Ready badge at "55 percent
    // opacity, 100 on hover". The tokens must carry those exact values so the
    // idle badge and the status word stay legible over the desktop.
    // (R-W2-003-002: exact pins, not floors — drift above the mockup is a
    // mockup violation and must fail the test, not slide by as headroom.)
    CHECK(metrics::mini_idle_alpha == 0.55f);
    CHECK(metrics::mini_active_alpha == 1.0f);
    // The window reserves room for the attention message at the wider width.
    CHECK(metrics::mini_message_w == 250.0f);
    // "Saved" dwell (mockup motion table: 1.5 s).
    CHECK(metrics::saved_dwell == 1.5f);
}

TEST_CASE("waveform ring SnapshotInto and At match Snapshot ordering") {
    OverlayModel::WaveformRing ring(64);
    for (int i = 0; i < 70; ++i) {
        ring.Push(static_cast<float>(i));
    }
    std::vector<float> scratch;
    ring.SnapshotInto(scratch);
    REQUIRE(scratch.size() == 64);
    CHECK(scratch.front() == 6.0f);
    CHECK(scratch.back() == 69.0f);
    // Snapshot must agree with SnapshotInto, sample for sample.
    CHECK(ring.Snapshot() == scratch);
    // At() reads oldest-first and throws past the newest sample.
    CHECK(ring.At(0) == 6.0f);
    CHECK(ring.At(63) == 69.0f);
    CHECK_THROWS_AS(ring.At(64), std::out_of_range);
    // Reuse: the same buffer stays order-stable after more pushes.
    ring.Push(100.0f);
    ring.SnapshotInto(scratch);
    REQUIRE(scratch.size() == 64);
    CHECK(scratch.front() == 7.0f);
    CHECK(scratch.back() == 100.0f);
}

TEST_CASE("mini status word is drawn at badge alpha without a discount") {
    // GUI-A6 regression guard (source-scan: lwf_tests does not link ImGui).
    // Before the fix the status word was Pack(muted, 0.75f * badge_alpha),
    // compounding the sub-1.0 token alphas. Require the badge-alpha draw
    // and forbid any sub-1.0 multiplier on the status word.
    std::ifstream overlay("src/ui/Overlay.cpp");
    REQUIRE(static_cast<bool>(overlay));
    const std::string content((std::istreambuf_iterator<char>(overlay)),
                              std::istreambuf_iterator<char>());
    CHECK(content.find("Pack(kDarkPalette.muted, badge_alpha)") != std::string::npos);
    // R-W2-003-003: forbid the CLASS, not one literal. "* badge_alpha" must
    // appear exactly once (the intended surface base dimming
    // `surface_alpha * badge_alpha` on the badge circle); any numeric
    // `0.xf * badge_alpha` discount makes it twice. "badge_alpha * " with a
    // trailing space catches the reversed operand order while excluding
    // the two legitimate `badge_alpha *= metrics::...` lines.
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count_occurrences(content, "* badge_alpha") == 1);
    CHECK(content.find("badge_alpha * ") == std::string::npos);
    // The attention message must render through the scoped 13px font.
    CHECK(content.find("ImGui::PushFont(message_font_, metrics::font_body_px)") !=
          std::string::npos);
    CHECK(content.find("ImGui::PopFont()") != std::string::npos);
    // R-W2-003-004: ordering inside DrawAttentionMessage — PushFont must wrap
    // both CalcTextSize and AddText (scoped to the function so the status-word
    // CalcTextSize in DrawBadge, which precedes it in the file, is excluded).
    const std::size_t fn = content.find("void Overlay::DrawAttentionMessage");
    REQUIRE(fn != std::string::npos);
    const std::string fn_body = content.substr(fn);
    const std::size_t push = fn_body.find("ImGui::PushFont(message_font_,");
    const std::size_t calc = fn_body.find("ImGui::CalcTextSize(");
    const std::size_t add = fn_body.find("draw.AddText(");
    const std::size_t pop = fn_body.find("ImGui::PopFont()");
    REQUIRE(push != std::string::npos);
    REQUIRE(calc != std::string::npos);
    REQUIRE(add != std::string::npos);
    REQUIRE(pop != std::string::npos);
    CHECK(push < calc);
    CHECK(calc < add);
    CHECK(add < pop);
}

TEST_CASE("waveform ring covers partial fill empty and pre-filled scratch") {
    // R-W2-003-005 + P2.1/P2.2: the filled < capacity branch (start = 0),
    // the zero-sample contract, and out.clear() over a pre-filled buffer.
    // Push 5 into cap 64: oldest-first order from the start-0 branch.
    OverlayModel::WaveformRing ring(64);
    for (int i = 0; i < 5; ++i) {
        ring.Push(static_cast<float>(i));
    }
    CHECK(ring.Size() == 5);
    std::vector<float> scratch;
    ring.SnapshotInto(scratch);
    REQUIRE(scratch.size() == 5);
    CHECK(scratch == std::vector<float>({0.0f, 1.0f, 2.0f, 3.0f, 4.0f}));
    CHECK(ring.At(0) == 0.0f);
    CHECK(ring.At(4) == 4.0f);
    CHECK_THROWS_AS(ring.At(5), std::out_of_range);
    // Pre-filled scratch: no sentinel survives, size is exactly filled_.
    std::vector<float> filled_scratch(100, -1.0f);
    ring.SnapshotInto(filled_scratch);
    REQUIRE(filled_scratch.size() == 5);
    CHECK(filled_scratch == std::vector<float>({0.0f, 1.0f, 2.0f, 3.0f, 4.0f}));
    // Empty ring: At throws (0 >= filled_ 0), SnapshotInto writes nothing.
    OverlayModel::WaveformRing empty(64);
    CHECK(empty.Size() == 0);
    CHECK_THROWS_AS(empty.At(0), std::out_of_range);
    std::vector<float> empty_scratch(3, -1.0f);
    empty.SnapshotInto(empty_scratch);
    CHECK(empty_scratch.empty());
}

TEST_CASE("mini idle early-out wakeup wiring is guarded") {
    // R-W2-002 / P2.3 source-scan guard (lwf_tests does not link ImGui): the
    // idle skip is only safe while every wakeup path keeps dirty_ set and the
    // poll verdicts cover inflated zone + exact dot + button state. Any dropped
    // setter below would freeze the badge without failing other tests.
    std::ifstream overlay("src/ui/Overlay.cpp");
    REQUIRE(static_cast<bool>(overlay));
    const std::string content((std::istreambuf_iterator<char>(overlay)),
                              std::istreambuf_iterator<char>());
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // Wakeups: SetState, PushLevel, UpdateDrag, DPI apply and the poll itself.
    CHECK(count_occurrences(content, "dirty_ = true") >= 3);
    // Font-cache invalidation at the DPI-apply sites (SetWorkArea + re-apply).
    CHECK(count_occurrences(content, "cached_status_valid_ = false") >= 1);
    // R-W2-002: exact-dot hover verdict + physical button poll in the idle
    // early-out; three IsCursorInsideRect sites (inflated, exact, DrawBadge).
    CHECK(content.find("poll_hover_dot_") != std::string::npos);
    CHECK(content.find("poll_hover_dot_ = poll_hover_dot") != std::string::npos);
    CHECK(content.find("LeftMouseButtonDown()") != std::string::npos);
    CHECK(count_occurrences(content, "IsCursorInsideRect(") >= 3);
}

TEST_CASE("main window keyboard nav and commit-on-deactivate wiring") {
    // R-W2-003-001 (MAJOR) / P1 source-scan guard for the A1/A3 delta:
    // lwf_tests does not link ImGui, so the MainWindow wiring is guarded
    // lexically, the same pattern as the wave-1 T-001 gate and the
    // Overlay scans above. Fail-before: 3aa4329^ contains none of
    // NavEnableKeyboard / copied_until_ and only the model-path
    // IsItemDeactivatedAfterEdit site, so every A1/A3 behavior marker below
    // fails pre-fix (the pre-existing stt_provider settings key is the lone
    // exception and guards the cloud branch against removal, not against the
    // pre-fix tree).
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string content((std::istreambuf_iterator<char>(main_window)),
                              std::istreambuf_iterator<char>());
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // A1: keyboard nav opt-in on the IO config and both InvisibleButtons
    // (ToggleSwitch + rail page buttons).
    CHECK(content.find("NavEnableKeyboard") != std::string::npos);
    CHECK(count_occurrences(content, "ImGuiButtonFlags_EnableNav") >= 2);
    // R-W2-001: the rail switches on the InvisibleButton activation return
    // (covers keyboard), never on mouse-only IsItemClicked.
    CHECK(content.find("const bool pressed =") != std::string::npos);
    CHECK(content.find("InvisibleButton(item.id") != std::string::npos);
    CHECK(content.find("if (ImGui::IsItemClicked(") == std::string::npos);
    // Focus restore is NavActive-gated and fires before SetKeyboardFocusHere
    // is handed to the next item.
    const std::size_t nav_active = content.find("NavActive");
    const std::size_t focus_here = content.find("SetKeyboardFocusHere");
    REQUIRE(nav_active != std::string::npos);
    REQUIRE(focus_here != std::string::npos);
    CHECK(nav_active < focus_here);
    // R-W2-003: only SetVisible and Welcome-Finish arm the restore; the rail
    // press must clear it (focus is already on the pressed item).
    CHECK(count_occurrences(content, "page_focus_pending_ = true") == 2);
    // A3: "Copied" dwell driven by copied_until_ and the shared token.
    CHECK(content.find("copied_until_") != std::string::npos);
    CHECK(content.find("\"Copied\"") != std::string::npos);
    // A3: commit-on-deactivate on model path + both cleanup sliders.
    CHECK(count_occurrences(content, "IsItemDeactivatedAfterEdit") >= 3);
    // A3: cleanup sliders must not be re-wrapped in per-tick
    // `if (SliderInt(...))` saves (the original bug).
    const std::size_t cleanup = content.find("void MainWindow::DrawCleanupPage");
    REQUIRE(cleanup != std::string::npos);
    CHECK(content.find("if (ImGui::SliderInt(", cleanup) == std::string::npos);
    // A3: validation errors clear on page switch / successful edit, and the
    // Welcome finish saves through ApplyAndSave.
    CHECK(count_occurrences(content, "validation_error_.clear()") >= 2);
    const std::size_t welcome = content.find("void MainWindow::DrawWelcomePage");
    REQUIRE(welcome != std::string::npos);
    CHECK(count_occurrences(content.substr(welcome), "ApplyAndSave()") >= 2);
    // Trust card cloud branch keys off the provider setting.
    CHECK(content.find("stt_provider") != std::string::npos);
    CHECK(content.find("openrouter_cleanup") != std::string::npos);
    CHECK(content.find("Cloud speech and OpenRouter cleanup are off.") != std::string::npos);
    const std::size_t llm_combo = content.find("##llm_provider");
    REQUIRE(llm_combo != std::string::npos);
    const std::size_t llm_apply = content.find("ApplyAndSave()", llm_combo);
    REQUIRE(llm_apply != std::string::npos);
    CHECK(content.find("restart_needed_ = true", llm_combo) < llm_apply);
    CHECK(content.find("DrawRestartBanner(scale)", cleanup) != std::string::npos);
}

TEST_CASE("studio navigation routes round-trip with Home fallback") {
    using Page = MainWindow::Page;
    const Page pages[] = {Page::Home, Page::Insights, Page::Dictionary, Page::Snippets,
                          Page::Transforms, Page::Scratchpad, Page::Settings};
    for (Page page : pages) {
        const std::string route = MainWindow::RouteName(page);
        CHECK(MainWindow::PageForRoute(route) == page);
    }
    CHECK(MainWindow::PageForRoute("Home") == Page::Home);
    CHECK(MainWindow::PageForRoute("Insights") == Page::Insights);
    CHECK(MainWindow::PageForRoute("Dictionary") == Page::Dictionary);
    CHECK(MainWindow::PageForRoute("Snippets") == Page::Snippets);
    CHECK(MainWindow::PageForRoute("Transforms") == Page::Transforms);
    CHECK(MainWindow::PageForRoute("Scratchpad") == Page::Scratchpad);
    CHECK(MainWindow::PageForRoute("Settings") == Page::Settings);
    CHECK(MainWindow::PageForRoute("Welcome") == Page::Home);
    CHECK(MainWindow::PageForRoute("Speech") == Page::Home);
    CHECK(MainWindow::PageForRoute("About") == Page::Home);
    CHECK(MainWindow::PageForRoute("") == Page::Home);
    CHECK(MainWindow::PageForRoute("bogus-route") == Page::Home);
    CHECK(MainWindow::PageForRoute("settings") == Page::Home);
    CHECK(std::string(MainWindow::RouteName(Page::Welcome)) == "Welcome");
    CHECK(MainWindow::PageForRoute(MainWindow::RouteName(Page::Welcome)) == Page::Home);
}

TEST_CASE("last_page setting defaults to Home and persists across reload") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_lastpage_settings.json").string();
    {
        std::ofstream file(path, std::ios::trunc);
        file << "{}";
    }
    {
        SettingsStore store(path);
        CHECK(store.HasKey("last_page"));
        CHECK(store.GetString("last_page") == "Home");
    }
    {
        SettingsStore store(path);
        CHECK(store.GetString("last_page") == "Home");
    }
    {
        SettingsStore store(path);
        store.SetString("last_page", "Insights");
        store.Save();
    }
    {
        SettingsStore store(path);
        CHECK(store.GetString("last_page") == "Insights");
    }
    {
        SettingsStore store(path);
        store.SetString("last_page", "Settings");
        store.Save();
    }
    {
        SettingsStore store(path);
        CHECK(store.GetString("last_page") == "Settings");
        CHECK(MainWindow::PageForRoute(store.GetString("last_page")) == MainWindow::Page::Settings);
    }
}

TEST_CASE("studio navigation rail and settings tab structure source-scan") {
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    std::ifstream header("src/ui/MainWindow.h");
    REQUIRE(static_cast<bool>(header));
    const std::string header_content((std::istreambuf_iterator<char>(header)),
                                     std::istreambuf_iterator<char>());
    CHECK(header_content.find("enum class Page") != std::string::npos);
    CHECK(header_content.find("Welcome, Home, Insights, Dictionary, Snippets, Transforms, Scratchpad, Settings") != std::string::npos);
    CHECK(header_content.find("Welcome, Home, Speech, Cloud, Cleanup, Privacy, About") == std::string::npos);
    CHECK(header_content.find("void DrawSettingsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawGeneralPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawInsightsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawDictionaryPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawSnippetsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawTransformsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawScratchpadPage(float scale)") != std::string::npos);
    CHECK(header_content.find("RouteName") != std::string::npos);
    CHECK(header_content.find("PageForRoute") != std::string::npos);

    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string content((std::istreambuf_iterator<char>(main_window)),
                              std::istreambuf_iterator<char>());
    CHECK(content.find("##nav_home") != std::string::npos);
    CHECK(content.find("##nav_insights") != std::string::npos);
    CHECK(content.find("##nav_dictionary") != std::string::npos);
    CHECK(content.find("##nav_snippets") != std::string::npos);
    CHECK(content.find("##nav_transforms") != std::string::npos);
    CHECK(content.find("##nav_scratchpad") != std::string::npos);
    CHECK(content.find("##nav_settings") != std::string::npos);
    CHECK(content.find("##nav_speech") == std::string::npos);
    CHECK(content.find("##nav_cloud") == std::string::npos);
    CHECK(content.find("##nav_cleanup") == std::string::npos);
    CHECK(content.find("##nav_privacy") == std::string::npos);
    CHECK(content.find("##nav_about") == std::string::npos);
    CHECK(content.find("##tab_general") != std::string::npos);
    CHECK(content.find("##tab_speech") != std::string::npos);
    CHECK(content.find("##tab_cloud") != std::string::npos);
    CHECK(content.find("##tab_cleanup") != std::string::npos);
    CHECK(content.find("##tab_privacy") != std::string::npos);
    CHECK(content.find("##tab_about") != std::string::npos);
    CHECK(count_occurrences(content, "Not available yet.") == 5);
    CHECK(content.find("kRailWidth") != std::string::npos);
    CHECK(content.find("40.0f * scale") != std::string::npos);
    CHECK(content.find("font_card_title_px") != std::string::npos);
    CHECK(content.find("PersistPage();") != std::string::npos);
    CHECK(content.find("last_page") != std::string::npos);
    CHECK(content.find("PageForRoute(settings_.GetString(\"last_page\"))") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawSettingsPage") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawGeneralPage") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawInsightsPage") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawDictionaryPage") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawSnippetsPage") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawTransformsPage") != std::string::npos);
    CHECK(content.find("void MainWindow::DrawScratchpadPage") != std::string::npos);
    CHECK(content.find("##card_general_hotkey") != std::string::npos);
    CHECK(content.find("StartLearnHotkey") != std::string::npos);
    CHECK(content.find("sound_enabled") != std::string::npos);
    CHECK(content.find("reduce_motion") != std::string::npos);
    CHECK(content.find("mini_x") != std::string::npos);

    std::ifstream store_file("src/core/SettingsStore.cpp");
    REQUIRE(static_cast<bool>(store_file));
    const std::string store_content((std::istreambuf_iterator<char>(store_file)),
                                    std::istreambuf_iterator<char>());
    CHECK(store_content.find("last_page") != std::string::npos);
    CHECK(store_content.find("\"Home\"") != std::string::npos);

    std::ifstream main_file("src/main.cpp");
    REQUIRE(static_cast<bool>(main_file));
    const std::string main_content((std::istreambuf_iterator<char>(main_file)),
                                   std::istreambuf_iterator<char>());
    CHECK(main_content.find("case UiCommandType::StartTransform") != std::string::npos);
    CHECK(main_content.find("case UiCommandType::OpenScratchpad") != std::string::npos);
}

TEST_CASE("studio palette keeps the provisional estimate token values") {
    using lwf::ui::kStudioPalette;
    CHECK(kStudioPalette.page_bg == 0xFAFAF8u);
    CHECK(kStudioPalette.rail_bg == 0xF4F2EEu);
    CHECK(kStudioPalette.card_bg == 0xF4F2EEu);
    CHECK(kStudioPalette.card_bg_white == 0xFFFFFFu);
    CHECK(kStudioPalette.border == 0xE7E4DEu);
    CHECK(kStudioPalette.ink == 0x1C1C1Cu);
    CHECK(kStudioPalette.muted == 0x6E6A62u);
    CHECK(kStudioPalette.teal_700 == 0x1E5E5Au);
    CHECK(kStudioPalette.teal_500 == 0x2F8077u);
    CHECK(kStudioPalette.teal_300 == 0x7DBFB7u);
    CHECK(kStudioPalette.teal_100 == 0xCDEBE7u);
    CHECK(kStudioPalette.heat_empty == 0xCDC6BBu);
    CHECK(kStudioPalette.banner_cream == 0xF1ECE4u);
}

TEST_CASE("studio palette layout tokens follow the shape scale") {
    namespace metrics = lwf::ui::studio_metrics;
    CHECK(metrics::radius_banner == 16.0f);
    CHECK(metrics::radius_card == 12.0f);
    CHECK(metrics::radius_row == 10.0f);
    CHECK(metrics::radius_input == 10.0f);
    CHECK(metrics::radius_button == 8.0f);
    CHECK(metrics::radius_chip == 6.0f);
    CHECK(metrics::radius_pill == 999.0f);
    CHECK(metrics::page_padding == 32.0f);
    CHECK(metrics::gap == 24.0f);
    CHECK(metrics::row_height == 52.0f);
    CHECK(metrics::hit_size == 32.0f);
    CHECK(metrics::font_banner_px == 40.0f);
    CHECK(metrics::font_caps_px == 12.0f);
    CHECK(metrics::font_page_title_px == 28.0f);
    CHECK(metrics::font_helper_px == 13.0f);
    CHECK(metrics::toast_dwell_ms == 5000);
    CHECK(metrics::toast_dwell_s == 5.0f);
    CHECK(metrics::toast_dwell_s * 1000.0f == static_cast<float>(metrics::toast_dwell_ms));
    CHECK(metrics::activity_cell == 14.0f);
    CHECK(metrics::activity_gap == 4.0f);
    CHECK(metrics::activity_corner == 3.0f);
    CHECK(metrics::gauge_stroke == 14.0f);
    CHECK(metrics::progress_height == 28.0f);
}

TEST_CASE("studio palette contrast passes on ink and teal pairs and muted on card fills") {
    using lwf::ui::ContrastRatio;
    using lwf::ui::kStudioPalette;
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.page_bg) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.rail_bg) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.card_bg) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.card_bg_white) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.banner_cream) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.teal_100) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.teal_300) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.card_bg_white, kStudioPalette.teal_700) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.card_bg_white, kStudioPalette.teal_500) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.teal_700, kStudioPalette.page_bg) >= 3.0);
    CHECK(ContrastRatio(kStudioPalette.teal_500, kStudioPalette.card_bg_white) >= 3.0);
    CHECK(ContrastRatio(kStudioPalette.teal_700, kStudioPalette.teal_100) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.muted, kStudioPalette.page_bg) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.muted, kStudioPalette.rail_bg) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.muted, kStudioPalette.card_bg) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.muted, kStudioPalette.card_bg_white) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.muted, kStudioPalette.banner_cream) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.border, kStudioPalette.page_bg) < 3.0);
    CHECK(ContrastRatio(kStudioPalette.border, kStudioPalette.card_bg_white) < 3.0);
    CHECK(ContrastRatio(kStudioPalette.teal_300, kStudioPalette.ink) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.card_bg_white, kStudioPalette.ink) >= 4.5);
    CHECK(ContrastRatio(kStudioPalette.teal_700, kStudioPalette.card_bg_white) >= 3.0);
    CHECK(ContrastRatio(kStudioPalette.teal_500, kStudioPalette.page_bg) >= 3.0);
    CHECK(ContrastRatio(kStudioPalette.muted, kStudioPalette.page_bg) >= 3.0);
    CHECK(ContrastRatio(kStudioPalette.ink, kStudioPalette.muted) >= 3.0);
}

TEST_CASE("studio portable helpers classify heat and clamp and space caps") {
    using lwf::ui::Clamp01;
    using lwf::ui::HeatColor;
    using lwf::ui::HeatLevel;
    using lwf::ui::kStudioPalette;
    using lwf::ui::UpperSpaced;
    CHECK(HeatLevel(0, 10) == 0);
    CHECK(HeatLevel(5, 0) == 0);
    CHECK(HeatLevel(0, 0) == 0);
    CHECK(HeatLevel(1, 10) == 1);
    CHECK(HeatLevel(2, 10) == 1);
    CHECK(HeatLevel(3, 10) == 1);
    CHECK(HeatLevel(6, 10) == 2);
    CHECK(HeatLevel(8, 10) == 3);
    CHECK(HeatLevel(10, 10) == 4);
    CHECK(HeatLevel(99, 10) == 4);
    CHECK(HeatLevel(25, 100) == 1);
    CHECK(HeatLevel(50, 100) == 2);
    CHECK(HeatLevel(75, 100) == 3);
    CHECK(HeatLevel(26, 100) == 1);
    CHECK(HeatLevel(51, 100) == 2);
    CHECK(HeatLevel(76, 100) == 3);
    const int month_maxima[] = {0, 1, 10, 100};
    for (const int month_max : month_maxima) {
        for (int value = -5; value <= 105; ++value) {
            CHECK(HeatLevel(value, month_max) ==
                  lwf::CalendarLevel(value, month_max));
        }
    }
    CHECK(HeatColor(0) == kStudioPalette.heat_empty);
    CHECK(HeatColor(1) == kStudioPalette.teal_100);
    CHECK(HeatColor(2) == kStudioPalette.teal_300);
    CHECK(HeatColor(3) == kStudioPalette.teal_500);
    CHECK(HeatColor(4) == kStudioPalette.teal_700);
    CHECK(HeatColor(9) == kStudioPalette.heat_empty);
    CHECK(Clamp01(2.0f) == 1.0f);
    CHECK(Clamp01(-0.5f) == 0.0f);
    CHECK(Clamp01(0.5f) == 0.5f);
    CHECK(UpperSpaced("Beta") == "B E T A");
    CHECK(UpperSpaced("") == "");
}

TEST_CASE("studio widgets respect the font mechanism and stay opt in") {
    const std::vector<std::string> studio_sources = {
        "src/ui/theme/StudioPalette.h",
        "src/ui/theme/StudioPalette.cpp",
        "src/ui/widgets/Widgets.h",
        "src/ui/widgets/Buttons.cpp",
        "src/ui/widgets/Display.cpp",
        "src/ui/widgets/Overlays.h",
        "src/ui/widgets/Overlays.cpp",
    };
    for (const std::string& path : studio_sources) {
        std::ifstream file(path);
        REQUIRE(static_cast<bool>(file));
        INFO(path);
        const std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        CHECK(content.find("SetWindowFontScale") == std::string::npos);
    }
    std::ifstream buttons("src/ui/widgets/Buttons.cpp");
    REQUIRE(static_cast<bool>(buttons));
    const std::string buttons_content((std::istreambuf_iterator<char>(buttons)),
                                      std::istreambuf_iterator<char>());
    CHECK(buttons_content.find("PushFont(nullptr,") != std::string::npos);
    CHECK(buttons_content.find("InvisibleButton") != std::string::npos);
    CHECK(buttons_content.find("safe_id") != std::string::npos);
    CHECK(buttons_content.find("safe_label") != std::string::npos);
    CHECK(buttons_content.find("safe_glyph") != std::string::npos);
    CHECK(buttons_content.find("safe_tab") != std::string::npos);
    std::ifstream display("src/ui/widgets/Display.cpp");
    REQUIRE(static_cast<bool>(display));
    const std::string display_content((std::istreambuf_iterator<char>(display)),
                                      std::istreambuf_iterator<char>());
    CHECK(display_content.find("safe_id") != std::string::npos);
    CHECK(display_content.find("w * clamped") != std::string::npos);
    CHECK(display_content.find("PushClipRect") != std::string::npos);
    CHECK(display_content.find("std::string(safe_id)") != std::string::npos);
    std::ifstream studio_cpp("src/ui/theme/StudioPalette.cpp");
    REQUIRE(static_cast<bool>(studio_cpp));
    const std::string studio_content((std::istreambuf_iterator<char>(studio_cpp)),
                                     std::istreambuf_iterator<char>());
    CHECK(studio_content.find("style = ImGuiStyle()") != std::string::npos);
    CHECK(studio_content.find("preserved_font_scale_dpi") != std::string::npos);
    std::ifstream overlays_h("src/ui/widgets/Overlays.h");
    REQUIRE(static_cast<bool>(overlays_h));
    const std::string overlays_h_content((std::istreambuf_iterator<char>(overlays_h)),
                                         std::istreambuf_iterator<char>());
    CHECK(overlays_h_content.find("bool visible_") != std::string::npos);
    std::ifstream overlays("src/ui/widgets/Overlays.cpp");
    REQUIRE(static_cast<bool>(overlays));
    const std::string overlays_content((std::istreambuf_iterator<char>(overlays)),
                                       std::istreambuf_iterator<char>());
    CHECK(overlays_content.find("BeginPopupModal") != std::string::npos);
    CHECK(overlays_content.find("ImGuiKey_Escape") != std::string::npos);
    CHECK(overlays_content.find("##toast_action") != std::string::npos);
    CHECK(overlays_content.find("Button(action_label_") == std::string::npos);
    CHECK(overlays_content.find("GetCursorScreenPos()") != std::string::npos);
    CHECK(overlays_content.find("SetCursorScreenPos(saved_cursor)") != std::string::npos);
    CHECK(overlays_content.find("if (!visible_)") != std::string::npos);
    CHECK(overlays_content.find("title == nullptr") != std::string::npos);
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string main_content((std::istreambuf_iterator<char>(main_window)),
                                   std::istreambuf_iterator<char>());
    CHECK(main_content.find("ApplyStudioStyle") == std::string::npos);
    std::ifstream overlay("src/ui/Overlay.cpp");
    REQUIRE(static_cast<bool>(overlay));
    const std::string overlay_content((std::istreambuf_iterator<char>(overlay)),
                                      std::istreambuf_iterator<char>());
    CHECK(overlay_content.find("ApplyStudioStyle") == std::string::npos);
}

TEST_CASE("every schema key has a control or a documented carve-out") {
    const std::vector<std::string> carve_outs = {"mini_x", "mini_y", "schema_version", "first_run"};
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string content((std::istreambuf_iterator<char>(main_window)),
                              std::istreambuf_iterator<char>());
    for (const SettingsStore::SchemaEntry& entry : SettingsStore::Schema()) {
        const std::string key = entry.key;
        INFO(key);
        bool carve_out = false;
        for (const std::string& candidate : carve_outs) {
            if (candidate == key) {
                carve_out = true;
            }
        }
        if (!carve_out) {
            CHECK(content.find("\"" + key + "\"") != std::string::npos);
        }
    }
}

TEST_CASE("new A4 schema keys default and clamp through the store") {
    const std::string path =
        (std::filesystem::temp_directory_path() / "lwf_a4_settings.json").string();
    {
        std::ofstream file(path, std::ios::trunc);
        file << "{}";
    }
    SettingsStore store(path);
    CHECK(store.HasKey("quiet_speech_boost"));
    CHECK(store.GetBool("quiet_speech_boost") == false);
    CHECK(store.HasKey("llm_provider"));
    CHECK(store.GetString("llm_provider") == "local");
    CHECK(store.IsHotApply("quiet_speech_boost"));
    CHECK_FALSE(store.IsHotApply("llm_provider"));
    store.SetBool("quiet_speech_boost", false);
    store.SetString("llm_provider", "openrouter");
    store.Save();
    SettingsStore reloaded(path);
    CHECK(reloaded.GetBool("quiet_speech_boost") == false);
    CHECK(reloaded.GetString("llm_provider") == "openrouter");
    store.SetInt("min_hold_ms", 999999);
    CHECK(store.GetInt("min_hold_ms") == 5000);
    store.SetDouble("vad_threshold", 9.0);
    CHECK(store.GetDouble("vad_threshold") == 1.0);
    store.SetInt("hotkey_code", -4);
    CHECK(store.GetInt("hotkey_code") == 0);
}

TEST_CASE("F9 widget sweep pins toggle, link, and popup invariants") {
    std::ifstream buttons("src/ui/widgets/Buttons.cpp");
    REQUIRE(static_cast<bool>(buttons));
    const std::string buttons_content((std::istreambuf_iterator<char>(buttons)),
                                      std::istreambuf_iterator<char>());
    CHECK(buttons_content.find("const float h = studio_metrics::hit_size * scale") !=
          std::string::npos);
    CHECK(buttons_content.find("value ? Pack(kStudioPalette.ink) : Pack(kStudioPalette.muted)") !=
          std::string::npos);
    std::ifstream overlays("src/ui/widgets/Overlays.cpp");
    REQUIRE(static_cast<bool>(overlays));
    const std::string overlays_content((std::istreambuf_iterator<char>(overlays)),
                                       std::istreambuf_iterator<char>());
    CHECK(overlays_content.find("if (ImGui::IsItemFocused())") != std::string::npos);
    std::size_t visible_resets = 0;
    std::size_t scan_from = 0;
    while ((scan_from = overlays_content.find("visible_ = false", scan_from)) !=
           std::string::npos) {
        ++visible_resets;
        ++scan_from;
    }
    CHECK(visible_resets == 4);
    std::ifstream transforms("src/ui/pages/TransformsPage.cpp");
    REQUIRE(static_cast<bool>(transforms));
    const std::string transforms_content((std::istreambuf_iterator<char>(transforms)),
                                         std::istreambuf_iterator<char>());
    CHECK(transforms_content.find("kCardRadius") == std::string::npos);
    CHECK(transforms_content.find("studio_metrics::radius_card * scale") != std::string::npos);
    CHECK(transforms_content.find("hovered ? kStudioPalette.ink : kStudioPalette.teal_700") !=
          std::string::npos);
    CHECK(transforms_content.find("ImGui::IsItemFocused()") != std::string::npos);
    std::ifstream palette_h("src/ui/theme/StudioPalette.h");
    REQUIRE(static_cast<bool>(palette_h));
    const std::string palette_content((std::istreambuf_iterator<char>(palette_h)),
                                      std::istreambuf_iterator<char>());
    CHECK(palette_content.find("HeatColor") != std::string::npos);
    std::ifstream display("src/ui/widgets/Display.cpp");
    REQUIRE(static_cast<bool>(display));
    const std::string display_content((std::istreambuf_iterator<char>(display)),
                                      std::istreambuf_iterator<char>());
    CHECK(display_content.find("using lwf::ui::HeatColor;") != std::string::npos);
    CHECK(display_content.find("case 4:") == std::string::npos);
    std::ifstream insights("src/ui/pages/InsightsPage.cpp");
    REQUIRE(static_cast<bool>(insights));
    const std::string insights_content((std::istreambuf_iterator<char>(insights)),
                                       std::istreambuf_iterator<char>());
    CHECK(insights_content.find("HeatPack") == std::string::npos);
    CHECK(insights_content.find("HeatColor(level)") != std::string::npos);
}

TEST_CASE("F9 copy lint bans internal terms and exclamation in display strings") {
    const std::vector<std::string> banned = {"oops", "fsm", "vad", "llm", "whisper", "provider",
                                             "providers", "pipeline", "pipelines", "stt"};
    const auto is_word = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_';
    };
    const auto contains_word = [&is_word](const std::string& text, const std::string& term) {
        std::size_t from = 0;
        while (true) {
            const std::size_t hit = text.find(term, from);
            if (hit == std::string::npos) {
                return false;
            }
            const bool left_ok = hit == 0 || !is_word(text[hit - 1]);
            const std::size_t after = hit + term.size();
            const bool right_ok = after >= text.size() || !is_word(text[after]);
            if (left_ok && right_ok) {
                return true;
            }
            from = hit + 1;
        }
    };
    const auto lower = [](std::string text) {
        for (char& c : text) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return text;
    };
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator("src/ui")) {
        const std::string path = entry.path().string();
        const std::string ext = entry.path().extension().string();
        if (ext != ".cpp" && ext != ".h") {
            continue;
        }
        std::ifstream file(path);
        REQUIRE(static_cast<bool>(file));
        const std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        std::size_t i = 0;
        while (i < content.size()) {
            if (content[i] != '"') {
                ++i;
                continue;
            }
            std::size_t j = i + 1;
            std::string literal;
            while (j < content.size() && content[j] != '"') {
                if (content[j] == '\\' && j + 1 < content.size()) {
                    ++j;
                }
                literal.push_back(content[j]);
                ++j;
            }
            i = j + 1;
            if (literal.empty()) {
                continue;
            }
            const std::size_t hidden = literal.find("##");
            if (hidden != std::string::npos) {
                literal.erase(hidden);
            }
            INFO(path << ": " << literal);
            CHECK(literal.find('!') == std::string::npos);
            const std::string lowered = lower(literal);
            for (const std::string& term : banned) {
                CHECK_FALSE(contains_word(lowered, term));
            }
        }
    }
}
