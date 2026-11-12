#include "core/InsightsModel.h"
#include "core/Languages.h"
#include "core/OverlayModel.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "ui/MainWindow.h"
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

TEST_CASE("theme palettes keep the hairline token values") {
    using lwf::ui::kDarkPalette;
    using lwf::ui::kLightPalette;
    CHECK(kDarkPalette.bg == 0x101214u);
    CHECK(kDarkPalette.surface == 0x17191Cu);
    CHECK(kDarkPalette.border == 0x2A2D31u);
    CHECK(kDarkPalette.text == 0xE8E9EAu);
    CHECK(kDarkPalette.muted == 0x9A9DA1u);
    CHECK(kDarkPalette.accent == 0xB8E356u);
    CHECK(kDarkPalette.err == 0xF08A8Fu);
    CHECK(kLightPalette.bg == 0xF7F7F5u);
    CHECK(kLightPalette.surface == 0xFFFFFFu);
    CHECK(kLightPalette.text == 0x1A1A18u);
    // Hairline accent A (DESIGN §4): per-theme derived lime, so
    // accent==brass==focus==0x3F6212 is the single-accent invariant (AC2).
    // Dark keeps its own lime 0xB8E356 (different value, same role).
    CHECK(kLightPalette.accent == 0x3F6212u);
    CHECK(kLightPalette.brass == 0x3F6212u);
    CHECK(kLightPalette.focus == 0x3F6212u);
    CHECK(kLightPalette.accent == kLightPalette.brass);
    CHECK(kLightPalette.accent == kLightPalette.focus);
    CHECK(kLightPalette.brass == kLightPalette.focus);
}

TEST_CASE("theme selection maps key values to palettes with graphite fallback") {
    // T2b: "paper" opts into kLightPalette; the default key value, empty
    // and any unknown string all fall back to kDarkPalette (Graphite),
    // matching the schema default "graphite".
    using lwf::ui::PaletteForTheme;
    using lwf::ui::kDarkPalette;
    using lwf::ui::kLightPalette;
    CHECK(&PaletteForTheme(lwf::ui::kThemeGraphite) == &kDarkPalette);
    CHECK(&PaletteForTheme("graphite") == &kDarkPalette);
    CHECK(&PaletteForTheme(lwf::ui::kThemePaper) == &kLightPalette);
    CHECK(&PaletteForTheme("paper") == &kLightPalette);
    CHECK(&PaletteForTheme("") == &kDarkPalette);
    CHECK(&PaletteForTheme("bogus") == &kDarkPalette);
}

TEST_CASE("theme palettes satisfy the contrast policy except the two hairline pairs") {
    using lwf::ui::ContrastRatio;
    for (const lwf::ui::Palette& palette : {lwf::ui::kDarkPalette, lwf::ui::kLightPalette}) {
        INFO((palette.bg == 0x101214u ? "dark" : "light"));
        // Body text: 4.5:1 on page background and card surface.
        CHECK(ContrastRatio(palette.text, palette.bg) >= 4.5);
        CHECK(ContrastRatio(palette.text, palette.surface) >= 4.5);
        // Helper text: 4.5:1.
        CHECK(ContrastRatio(palette.muted, palette.bg) >= 4.5);
        CHECK(ContrastRatio(palette.muted, palette.surface) >= 4.5);
        // Meaningful colors: 4.5:1 (Hairline DESIGN §5 passes them at body level).
        CHECK(ContrastRatio(palette.accent, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.ok, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.warn, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.err, palette.surface) >= 4.5);
        CHECK(ContrastRatio(palette.brass, palette.surface) >= 4.5);
        // Text on accent fills (primary buttons) at 4.5:1.
        CHECK(ContrastRatio(palette.on_accent, palette.accent) >= 4.5);
        // Meaningful non-text (focus ring, error dot fill) at 3:1. err_fill
        // is FILL-ONLY, never text: it is asserted at 3.0 here and must never
        // be asserted at 4.5 as text (DESIGN §2: state never color-alone).
        CHECK(ContrastRatio(palette.focus, palette.surface) >= 3.0);
        CHECK(ContrastRatio(palette.err_fill, palette.surface) >= 3.0);
        // The two decorative hairline pairs are the documented exemption:
        // they sit below 3:1 by design and never carry state on their own.
        CHECK(ContrastRatio(palette.border, palette.bg) < 3.0);
        CHECK(ContrastRatio(palette.border, palette.surface) < 3.0);
    }
}

TEST_CASE("contrast math matches the WCAG formula") {
    using lwf::ui::ContrastRatio;
    CHECK(std::abs(ContrastRatio(0xE8E9EA, 0x101214) - 15.44) < 0.02);
    CHECK(std::abs(ContrastRatio(0xE8E9EA, 0x17191C) - 14.49) < 0.02);
    CHECK(std::abs(ContrastRatio(0xB8E356, 0x17191C) - 11.88) < 0.02);
    CHECK(std::abs(ContrastRatio(0x1A1A18, 0xF7F7F5) - 16.25) < 0.02);
}

TEST_CASE("design px font tokens follow the hairline type scale") {
    namespace metrics = lwf::ui::metrics;
    // Hairline type scale (DESIGN §3.4): title 24 / section 14 / body 13 /
    // helper 12. Token names are kept (values only): wordmark/page_title
    // are title, card_title is section, body is body, mini is helper.
    // T4a: the rail token is retired with the rail (no metrics::rail_width).
    // fade_in 0.12 -> 0.15 (functional motion band).
    CHECK(metrics::fade_in == 0.15f);
    CHECK(metrics::radius_card == 10.0f);
    CHECK(metrics::topstrip_height == 48.0f);
    CHECK(metrics::status_square == 8.0f);
    // Hairline design mapping: title 24 (wordmark/page title),
    // section 14 (card title), body 13, helper 12 (mini status label).
    CHECK(metrics::font_wordmark_px == 24.0f);
    CHECK(metrics::font_page_title_px == 24.0f);
    CHECK(metrics::font_card_title_px == 14.0f);
    CHECK(metrics::font_body_px == 13.0f);
    CHECK(metrics::font_mini_text_px == 12.0f);
}

namespace {

std::string ReadUiSource(const char* path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace

// Source pin (followups-2): the user-visible app name is "Echo flow" on the
// wordmark, window title, About row, tray tooltip/notification and the
// crash/log headers. Internal identifiers (window class names, registry run
// value, data dir, HTTP user agent) intentionally keep the LocalWisperFlow
// spelling so existing installs keep their config and data.
TEST_CASE("user-visible branding is Echo flow while internal identifiers stay LocalWisperFlow") {
    const std::string window_src = ReadUiSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(window_src.empty());
    CHECK(window_src.find("CalcTextSize(\"Echo flow\")") != std::string::npos);
    CHECK(window_src.find("Pack(current_palette_->text), \"Echo flow\")") != std::string::npos);
    CHECK(window_src.find("\"Echo flow\", nullptr, nullptr)") != std::string::npos);
    CHECK(window_src.find("Row(\"##card_about\", \"Echo flow\"") != std::string::npos);
    CHECK(window_src.find("Local Wisper Flow") == std::string::npos);

    const std::string tray_src = ReadUiSource("src/os/WindowsTray.cpp");
    REQUIRE_FALSE(tray_src.empty());
    CHECK(tray_src.find("L\"Echo flow\"") != std::string::npos);
    CHECK(tray_src.find("L\"Local Wisper Flow\"") == std::string::npos);
    // The registry run value is an internal identifier and must not move.
    CHECK(tray_src.find("kRunValue") != std::string::npos);

    const std::string crash_src = ReadUiSource("src/core/CrashReporter.cpp");
    REQUIRE_FALSE(crash_src.empty());
    CHECK(crash_src.find("\"app: Echo flow \"") != std::string::npos);

    const std::string paths_src = ReadUiSource("src/os/DataDir.cpp");
    REQUIRE_FALSE(paths_src.empty());
    CHECK(paths_src.find("LocalWisperFlow") != std::string::npos);
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
        "src/ui/pages/SnippetsPage.h",
        "src/ui/pages/SnippetsPage.cpp",
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

    // Gate markers (R-W1-003-001, T2b per-context fix): ApplyImGuiStyle gates
    // ScaleAllSizes (cumulative / lossy) once per actual change per ImGui
    // context via g_last_scale_by_ctx. A single global (context, scale) pair
    // cannot gate three contexts: at >100% DPI each cross-window theme toggle
    // re-ran ScaleAllSizes and drifted _MainScale-derived fields (100% drifted
    // zero, hiding it). The old globals must stay absent so a revert fails.
    std::ifstream palette_cpp("src/ui/theme/ThemePalette.cpp");
    REQUIRE(static_cast<bool>(palette_cpp));
    const std::string palette_content((std::istreambuf_iterator<char>(palette_cpp)),
                                      std::istreambuf_iterator<char>());
    CHECK(palette_content.find("g_last_scale_by_ctx") != std::string::npos);
    CHECK(palette_content.find("g_last_scale_by_ctx.find") != std::string::npos);
    CHECK(palette_content.find("g_last_scale_by_ctx.end()") != std::string::npos);
    CHECK(palette_content.find("g_last_scale_by_ctx[") != std::string::npos);
    CHECK(palette_content.find("!= scale") != std::string::npos);
    CHECK(palette_content.find("g_last_applied_context") == std::string::npos);
    CHECK(palette_content.find("g_last_applied_scale") == std::string::npos);
    CHECK(palette_content.find("const bool applies =") != std::string::npos);
    CHECK(palette_content.find("if (applies)") != std::string::npos);
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
    // The attention expansion keeps the wider message reserve.
    CHECK(metrics::mini_message_w == 250.0f);
    // Followups D6: the normal window hugs the icons — dot + status strip
    // with ring_pad clearance (38x48 at 1.0 scale; the attention state
    // expands to 250 wide).
    CHECK(metrics::mini_dot == 22.0f);
    CHECK(metrics::mini_ring_pad == 8.0f);
    CHECK(metrics::mini_status_h == 18.0f);
    CHECK(2.0f * metrics::mini_ring_pad + metrics::mini_dot == 38.0f);
    CHECK(metrics::mini_ring_pad + metrics::mini_dot + metrics::mini_status_h == 48.0f);
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
    CHECK(content.find("Pack(current_palette_->muted, badge_alpha)") != std::string::npos);
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
    // A1: keyboard nav opt-in on the IO config. EnableNav now lives inside
    // TopStripItem (src/ui/widgets/Buttons.cpp, pinned by the T3 widget
    // sweep) and the file-local ToggleSwitch; every strip destination is
    // keyboard-focusable through its TopStripItem call site below.
    CHECK(content.find("NavEnableKeyboard") != std::string::npos);
    CHECK(content.find("lwf::ui::widgets::TopStripItem") != std::string::npos);
    CHECK(count_occurrences(content, "##strip_") >= 7);
    // m1: EnableNav must live inside TopStripItem itself
    // (src/ui/widgets/Buttons.cpp), not merely somewhere in the file. Anchor
    // on the function and require the flag after it and before its return,
    // so deleting the flag from TopStripItem turns this pin red even though
    // other widgets in the same file keep their own EnableNav.
    std::ifstream buttons_top("src/ui/widgets/Buttons.cpp");
    REQUIRE(static_cast<bool>(buttons_top));
    const std::string buttons_content((std::istreambuf_iterator<char>(buttons_top)),
                                      std::istreambuf_iterator<char>());
    const std::size_t topstrip_fn = buttons_content.find("bool TopStripItem");
    REQUIRE(topstrip_fn != std::string::npos);
    const std::size_t enable_nav =
        buttons_content.find("ImGuiButtonFlags_EnableNav", topstrip_fn);
    REQUIRE(enable_nav != std::string::npos);
    const std::size_t topstrip_return =
        buttons_content.find("return pressed;", topstrip_fn);
    REQUIRE(topstrip_return != std::string::npos);
    CHECK(enable_nav < topstrip_return);
    // R-W2-001: the strip switches on the TopStripItem activation return
    // (covers keyboard), never on mouse-only IsItemClicked.
    CHECK(content.find("const bool pressed =") != std::string::npos);
    CHECK(content.find("TopStripItem(item.id") != std::string::npos);
    CHECK(content.find("if (ImGui::IsItemClicked(") == std::string::npos);
    // Focus restore is NavActive-gated and fires before SetKeyboardFocusHere
    // is handed to the next item.
    const std::size_t nav_active = content.find("NavActive");
    const std::size_t focus_here = content.find("SetKeyboardFocusHere");
    REQUIRE(nav_active != std::string::npos);
    REQUIRE(focus_here != std::string::npos);
    CHECK(nav_active < focus_here);
    // R-W2-003: only SetVisible and Welcome-Finish arm the restore; the strip
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
    const Page pages[] = {Page::Home, Page::History, Page::Insights, Page::Dictionary, Page::Snippets,
                          Page::Transforms, Page::Profiles, Page::Settings};
    for (Page page : pages) {
        const std::string route = MainWindow::RouteName(page);
        CHECK(MainWindow::PageForRoute(route) == page);
    }
    CHECK(MainWindow::PageForRoute("Home") == Page::Home);
    CHECK(MainWindow::PageForRoute("History") == Page::History);
    CHECK(MainWindow::PageForRoute("Insights") == Page::Insights);
    CHECK(MainWindow::PageForRoute("Dictionary") == Page::Dictionary);
    CHECK(MainWindow::PageForRoute("Snippets") == Page::Snippets);
    CHECK(MainWindow::PageForRoute("Transforms") == Page::Transforms);
    CHECK(MainWindow::PageForRoute("Profiles") == Page::Profiles);
    CHECK(MainWindow::PageForRoute("Settings") == Page::Settings);
    CHECK(MainWindow::PageForRoute("Welcome") == Page::Home);
    CHECK(MainWindow::PageForRoute("Speech") == Page::Home);
    CHECK(MainWindow::PageForRoute("About") == Page::Home);
    // The Scratchpad page was removed outright; a stale persisted route
    // (last_page = "Scratchpad" from an older config) must fall back Home.
    CHECK(MainWindow::PageForRoute("Scratchpad") == Page::Home);
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
    CHECK(header_content.find("Welcome, Home, History, Insights, Dictionary, Snippets, Transforms, Profiles, Settings") != std::string::npos);
    CHECK(header_content.find("Scratchpad") == std::string::npos);
    CHECK(header_content.find("Welcome, Home, Speech, Cloud, Cleanup, Privacy, About") == std::string::npos);
    CHECK(header_content.find("void DrawSettingsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawGeneralPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawInsightsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawDictionaryPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawSnippetsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawTransformsPage(float scale)") != std::string::npos);
    CHECK(header_content.find("void DrawScratchpadPage(float scale)") == std::string::npos);
    CHECK(header_content.find("void DrawProfilesPage(float scale)") != std::string::npos);
    CHECK(header_content.find("RouteName") != std::string::npos);
    CHECK(header_content.find("PageForRoute") != std::string::npos);

    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string content((std::istreambuf_iterator<char>(main_window)),
                              std::istreambuf_iterator<char>());
    CHECK(content.find("##strip_home") != std::string::npos);
    CHECK(content.find("##strip_history") != std::string::npos);
    CHECK(content.find("##strip_insights") != std::string::npos);
    CHECK(content.find("##strip_dictionary") != std::string::npos);
    CHECK(content.find("##strip_snippets") != std::string::npos);
    CHECK(content.find("##strip_transforms") != std::string::npos);
    CHECK(content.find("##strip_scratchpad") == std::string::npos);
    CHECK(content.find("##strip_profiles") != std::string::npos);
    CHECK(content.find("##strip_settings") != std::string::npos);
    CHECK(content.find("##nav_home") == std::string::npos);
    CHECK(content.find("##nav_insights") == std::string::npos);
    CHECK(content.find("##nav_dictionary") == std::string::npos);
    CHECK(content.find("##nav_snippets") == std::string::npos);
    CHECK(content.find("##nav_transforms") == std::string::npos);
    CHECK(content.find("##nav_settings") == std::string::npos);
    CHECK(content.find("##strip_speech") == std::string::npos);
    CHECK(content.find("##strip_cloud") == std::string::npos);
    CHECK(content.find("##strip_cleanup") == std::string::npos);
    CHECK(content.find("##strip_privacy") == std::string::npos);
    CHECK(content.find("##strip_about") == std::string::npos);
    CHECK(content.find("DrawRail") == std::string::npos);
    CHECK(content.find("##rail") == std::string::npos);
    CHECK(content.find("void MainWindow::DrawTopStrip") != std::string::npos);
    CHECK(content.find("topstrip_height * scale") != std::string::npos);
    CHECK(content.find("##tab_general") != std::string::npos);
    CHECK(content.find("##tab_speech") != std::string::npos);
    CHECK(content.find("##tab_cloud") != std::string::npos);
    CHECK(content.find("##tab_cleanup") != std::string::npos);
    CHECK(content.find("##tab_privacy") != std::string::npos);
    CHECK(content.find("##tab_about") != std::string::npos);
    CHECK(count_occurrences(content, "Not available yet.") == 6);
    CHECK(content.find("kRailWidth") == std::string::npos);
    CHECK(content.find("40.0f * scale") == std::string::npos);
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
    CHECK(content.find("void MainWindow::DrawScratchpadPage") == std::string::npos);
    CHECK(content.find("void MainWindow::DrawProfilesPage") != std::string::npos);
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
    CHECK(main_content.find("case UiCommandType::OpenScratchpad") == std::string::npos);
}

TEST_CASE("top strip status line and Ctrl-digit shortcuts source-scan") {
    // T4b-owned pins only (status/shortcut presence + mapping values); the
    // nav/route/persistence/grid/widget pins above keep their own cases.
    // lwf_tests does not link ImGui, so the wiring is guarded lexically,
    // scoped to the DrawTopStrip section so a Home-page lookalike string
    // cannot satisfy the pins by accident.
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string content((std::istreambuf_iterator<char>(main_window)),
                              std::istreambuf_iterator<char>());
    const std::size_t strip_fn = content.find("void MainWindow::DrawTopStrip");
    REQUIRE(strip_fn != std::string::npos);
    const std::size_t strip_end = content.find("void MainWindow::DrawPageChrome", strip_fn);
    REQUIRE(strip_end != std::string::npos);
    const std::string status_section = content.substr(strip_fn, strip_end - strip_fn);
    // StatusSquare call site with the live palette (never the Paper
    // default, which would break Graphite). Scoped to the call itself:
    // a bare `*current_palette_` search over the whole section is
    // satisfied by the DECISION comment alone (reviewer mutation-proved).
    CHECK(status_section.find("lwf::ui::widgets::StatusSquare") != std::string::npos);
    {
        const std::size_t call = status_section.find("StatusSquare(\"##strip_status\"");
        REQUIRE(call != std::string::npos);
        const std::string window = status_section.substr(call, 200);
        CHECK(window.find("*current_palette_") != std::string::npos);
    }
    // Reserve growth: space_xl*4 (128 design px).
    CHECK(status_section.find("space_xl * 4.0f") != std::string::npos);
    // Mapping words.
    CHECK(status_section.find("\"Ready\"") != std::string::npos);
    CHECK(status_section.find("\"Listening\"") != std::string::npos);
    CHECK(status_section.find("\"Thinking\"") != std::string::npos);
    CHECK(status_section.find("\"Paused\"") != std::string::npos);
    CHECK(status_section.find("\"Attention\"") != std::string::npos);
    // Locality phrases.
    CHECK(status_section.find("\"Local\"") != std::string::npos);
    CHECK(status_section.find("\"Cloud: Gnani\"") != std::string::npos);
    CHECK(status_section.find("\"Cloud: ElevenLabs\"") != std::string::npos);
    CHECK(status_section.find("\"Cloud: Muse\"") != std::string::npos);
    CHECK(status_section.find("phrase = \"Cloud\"") != std::string::npos);
    CHECK(status_section.find("\"OpenRouter\"") != std::string::npos);
    // Mapping-table values (palette-role vocabulary, never FSM names).
    CHECK(status_section.find("StatusKind::Ok") != std::string::npos);
    CHECK(status_section.find("StatusKind::Accent") != std::string::npos);
    CHECK(status_section.find("StatusKind::Muted") != std::string::npos);
    CHECK(status_section.find("StatusKind::Err") != std::string::npos);
    // Source conditions behind the table.
    CHECK(status_section.find("validation_error_") != std::string::npos);
    CHECK(status_section.find("dictation_enabled") != std::string::npos);
    CHECK(status_section.find("State::CAPTURING") != std::string::npos);
    CHECK(status_section.find("State::TRANSCRIBING") != std::string::npos);
    CHECK(status_section.find("State::POSTPROCESSING") != std::string::npos);
    CHECK(status_section.find("State::EMITTING") != std::string::npos);
    CHECK(status_section.find("stt_consent_accepted") != std::string::npos);
    // Ctrl+digit handling with the text-input guard.
    CHECK(status_section.find("ImGuiKey_1") != std::string::npos);
    CHECK(status_section.find("ImGuiKey_7") != std::string::npos);
    // Full GetIO() spelling: a bare `WantTextInput` search is satisfied by
    // the guard comment alone (reviewer mutation-proved).
    CHECK(status_section.find("GetIO().WantTextInput") != std::string::npos);
    CHECK(status_section.find("ImGuiMod_Ctrl") != std::string::npos);
    // Shared press path: definition + strip press + shortcut press all
    // land in ActivateStripPage (one function, not copy-pasted logic).
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    CHECK(count_occurrences(content, "ActivateStripPage") >= 3);
    std::ifstream main_header("src/ui/MainWindow.h");
    REQUIRE(static_cast<bool>(main_header));
    const std::string header_content((std::istreambuf_iterator<char>(main_header)),
                                     std::istreambuf_iterator<char>());
    CHECK(header_content.find("ActivateStripPage") != std::string::npos);
}

TEST_CASE("hairline palette keeps the primary token values") {
    // Hairline: the deprecated shim struct is deleted; pin the
    // Paper kLightPalette values directly (DESIGN.md §3.2). The
    // HeatColor ramp stops are pinned via HeatColor (see helpers test).
    using lwf::ui::kLightPalette;
    CHECK(kLightPalette.bg == 0xF7F7F5u);
    CHECK(kLightPalette.surface == 0xFFFFFFu);
    CHECK(kLightPalette.border == 0xE2E2DFu);
    CHECK(kLightPalette.text == 0x1A1A18u);
    CHECK(kLightPalette.muted == 0x6B6B66u);
    CHECK(kLightPalette.accent == 0x3F6212u);
    CHECK(kLightPalette.brass == 0x3F6212u);
    CHECK(kLightPalette.focus == 0x3F6212u);
    // Single-accent invariant: accent == brass == focus.
    CHECK(kLightPalette.accent == kLightPalette.brass);
    CHECK(kLightPalette.accent == kLightPalette.focus);
}

TEST_CASE("studio metrics layout tokens follow the shape scale") {
    namespace metrics = lwf::ui::studio_metrics;
    CHECK(metrics::radius_banner == 16.0f);
    CHECK(metrics::radius_card == 10.0f);
    CHECK(metrics::radius_row == 10.0f);
    CHECK(metrics::radius_input == 10.0f);
    CHECK(metrics::radius_button == 8.0f);
    CHECK(metrics::radius_chip == 6.0f);
    CHECK(metrics::radius_pill == 999.0f);
    CHECK(metrics::page_padding == 32.0f);
    CHECK(metrics::gap == 24.0f);
    CHECK(metrics::row_height == 52.0f);
    CHECK(metrics::hit_size == 32.0f);
    CHECK(metrics::font_banner_px == 24.0f);
    CHECK(metrics::font_caps_px == 12.0f);
    CHECK(metrics::font_page_title_px == 24.0f);
    CHECK(metrics::font_helper_px == 12.0f);
    CHECK(metrics::toast_dwell_ms == 5000);
    CHECK(metrics::toast_dwell_s == 5.0f);
    CHECK(metrics::toast_dwell_s * 1000.0f == static_cast<float>(metrics::toast_dwell_ms));
    CHECK(metrics::activity_cell == 14.0f);
    CHECK(metrics::activity_gap == 4.0f);
    CHECK(metrics::activity_corner == 3.0f);
    CHECK(metrics::gauge_stroke == 14.0f);
    CHECK(metrics::progress_height == 28.0f);
    CHECK(metrics::font_card_title_px == 14.0f);
    CHECK(metrics::font_heading_px == 24.0f);
    CHECK(metrics::font_subheading_px == 14.0f);
    CHECK(metrics::font_glyph_px == 10.0f);
    CHECK(metrics::gap_micro == 6.0f);
    CHECK(metrics::gap_inline == 8.0f);
    CHECK(metrics::gap_compact == 10.0f);
    CHECK(metrics::gap_section == 12.0f);
    CHECK(metrics::gap_block == 16.0f);
}

TEST_CASE("hairline palette contrast passes on text and accent pairs and muted on card fills") {
    // Hairline: the shim fields now resolve to the Paper
    // kLightPalette fields (and HeatColor for the ramp stage). Thresholds
    // are otherwise unchanged (4.5 text / 3.0 meaningful / hairline <3.0).
    // Two honest adjustments, each commented:
    //  - accent vs border is now accent(0x3F6212) vs border(0xE2E2DF)
    //    = 5.45 — moved to the non-text >=3.0 bucket (DESIGN §10: light
    //    ramp steps are decorative, never text; ink text on the light ramp
    //    stays at >=4.5 below).
    //  - ink vs muted (16.25/5.36 text values on their fills) is 3.25 and
    //    is REMOVED as a pair: muted is never rendered on ink in any view
    //    (DESIGN §2 heat note: state never color-alone). Every real
    //    adjacency (muted on page/rail/card/banner, ink on ramp tints)
    //    is asserted below at its true threshold.
    using lwf::ui::ContrastRatio;
    using lwf::ui::HeatColor;
    using lwf::ui::kLightPalette;
    // 1:1 retarget of the shim fields: page_bg->bg, rail_bg/card_bg/
    // card_bg_white/banner_cream->surface, teal_700/teal_500->accent,
    // teal_100->border, teal_300->HeatColor(3), ink->text, muted->muted.
    CHECK(ContrastRatio(kLightPalette.text, kLightPalette.bg) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.text, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.text, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.text, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.text, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.text, kLightPalette.border) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.text, HeatColor(3)) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.surface, kLightPalette.accent) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.surface, kLightPalette.accent) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.accent, kLightPalette.bg) >= 3.0);
    CHECK(ContrastRatio(kLightPalette.accent, kLightPalette.surface) >= 3.0);
    CHECK(ContrastRatio(kLightPalette.accent, kLightPalette.border) >= 3.0);
    CHECK(ContrastRatio(kLightPalette.muted, kLightPalette.bg) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.muted, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.muted, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.muted, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.muted, kLightPalette.surface) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.border, kLightPalette.bg) < 3.0);
    CHECK(ContrastRatio(kLightPalette.border, kLightPalette.surface) < 3.0);
    CHECK(ContrastRatio(HeatColor(3), kLightPalette.text) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.surface, kLightPalette.text) >= 4.5);
    CHECK(ContrastRatio(kLightPalette.accent, kLightPalette.surface) >= 3.0);
    CHECK(ContrastRatio(kLightPalette.accent, kLightPalette.bg) >= 3.0);
    CHECK(ContrastRatio(kLightPalette.muted, kLightPalette.bg) >= 3.0);
}

TEST_CASE("studio portable helpers classify heat and clamp and space caps") {
    using lwf::ui::Clamp01;
    using lwf::ui::HeatColor;
    using lwf::ui::HeatLevel;
    using lwf::ui::kLightPalette;
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
    // Lime ramp over surface (DESIGN §10) — HeatLevel math is
    // unchanged, only the color stops moved from brass to lime. L4/L0 equal
    // accent/border by the Hairline mapping; L1/L2 are ramp-only stops
    // (0xC9CCC4/0xABB1A4). Numeric count + legend always accompany cells
    // (state never color-alone).
    CHECK(HeatColor(0) == 0xE2E2DFu);
    CHECK(HeatColor(1) == 0xC9CCC4u);
    CHECK(HeatColor(2) == 0xABB1A4u);
    CHECK(HeatColor(3) == 0x839178u);
    CHECK(HeatColor(4) == 0x3F6212u);
    CHECK(HeatColor(9) == 0xE2E2DFu);
    CHECK(HeatColor(0) == kLightPalette.border);
    CHECK(HeatColor(3) == 0x839178u);
    CHECK(HeatColor(4) == kLightPalette.accent);
    // DESIGN §10: ramp stages 3-4 meet >=3.0 against the surface; stages
    // 0-2 are sub-3 surfaces whose state is carried by the numeric label.
    CHECK(lwf::ui::ContrastRatio(HeatColor(3), kLightPalette.surface) >= 3.0);
    CHECK(lwf::ui::ContrastRatio(HeatColor(4), kLightPalette.surface) >= 3.0);
    CHECK(Clamp01(2.0f) == 1.0f);
    CHECK(Clamp01(-0.5f) == 0.0f);
    CHECK(Clamp01(0.5f) == 0.5f);
    CHECK(UpperSpaced("Beta") == "B E T A");
    CHECK(UpperSpaced("") == "");
}

TEST_CASE("studio widgets respect the font mechanism and stay opt in") {
    const std::vector<std::string> studio_sources = {
        "src/ui/theme/ThemePalette.h",
        "src/ui/theme/ThemePalette.cpp",
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
    // Font mechanism lives in ApplyImGuiStyle (ThemePalette.cpp SSOT):
    // DPI factor applied once via FontScaleMain + ScaleAllSizes, so a
    // re-apply cannot drift; no fresh-style reset exists in the single
    // palette (geometry is assigned in place).
    std::ifstream theme_cpp("src/ui/theme/ThemePalette.cpp");
    REQUIRE(static_cast<bool>(theme_cpp));
    const std::string theme_content((std::istreambuf_iterator<char>(theme_cpp)),
                                    std::istreambuf_iterator<char>());
    CHECK(theme_content.find("void ApplyImGuiStyle") != std::string::npos);
    CHECK(theme_content.find("style.FontScaleMain = scale;") != std::string::npos);
    CHECK(theme_content.find("style.ScaleAllSizes(scale);") != std::string::npos);
    CHECK(theme_content.find("SetWindowFontScale") == std::string::npos);
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
    CHECK(overlays_content.find("ImGui::SetCursorScreenPos(ImVec2(saved_cursor.x, saved_cursor.y - ImGui::GetStyle().ItemSpacing.y));") != std::string::npos);
    CHECK(overlays_content.find("if (!visible_)") != std::string::npos);
    CHECK(overlays_content.find("title == nullptr") != std::string::npos);
    std::ifstream main_window("src/ui/MainWindow.cpp");
    REQUIRE(static_cast<bool>(main_window));
    const std::string main_content((std::istreambuf_iterator<char>(main_window)),
                                   std::istreambuf_iterator<char>());
    // T2b: the style is chosen via PaletteForTheme (graphite default); the
    // direct Pack(kLightPalette...) custom draws are T3-owned and unchanged.
    CHECK(main_content.find("PaletteForTheme") != std::string::npos);
    std::ifstream overlay("src/ui/Overlay.cpp");
    REQUIRE(static_cast<bool>(overlay));
    const std::string overlay_content((std::istreambuf_iterator<char>(overlay)),
                                      std::istreambuf_iterator<char>());
    CHECK(overlay_content.find("PaletteForTheme") != std::string::npos);
}

TEST_CASE("every schema key has a control or a documented carve-out") {
    const std::vector<std::string> carve_outs = {"mini_x", "mini_y", "schema_version", "first_run", "hotkey_code"};
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
    // T2b additive schema key: default Graphite, live-applied.
    CHECK(store.HasKey("theme"));
    CHECK(store.GetString("theme") == "graphite");
    CHECK(store.IsHotApply("theme"));
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
    // T3 hairline toggle: the filled text/muted track is gone; the knob is a
    // square sized from metrics::status_square — on-state accent square,
    // off-state muted hairline square, hairline track (AddRect outline).
    CHECK(buttons_content.find("value ? Pack(palette.text) : Pack(palette.muted)") ==
          std::string::npos);
    CHECK(buttons_content.find("metrics::status_square * scale") != std::string::npos);
    CHECK(buttons_content.find("draw.AddRect(p, ImVec2(p.x + w, p.y + h)") != std::string::npos);
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
    // T10c link-hover pair retired with the banner's TextLink (D3); the
    // live palette stays pinned by the rows below.
    CHECK(transforms_content.find("kLightPalette") == std::string::npos);
    CHECK(transforms_content.find("Color(palette.err), \"%s\", edit_error_.c_str()") !=
          std::string::npos);
    CHECK(transforms_content.find("ImGui::IsItemFocused()") != std::string::npos);
    std::ifstream palette_h("src/ui/theme/ThemePalette.h");
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
    // T3 new widgets + deletion of the card primitives (DESIGN §6: no
    // card/filled-panel primitive remains). TopStripItem carries the 48 px
    // top-strip token, StatusSquare the 8 px status-square token, the ruled
    // Row the 52 px row token — all hairline-drawn (no child, no fill).
    std::ifstream widgets_h("src/ui/widgets/Widgets.h");
    REQUIRE(static_cast<bool>(widgets_h));
    const std::string widgets_h_content((std::istreambuf_iterator<char>(widgets_h)),
                                        std::istreambuf_iterator<char>());
    CHECK(widgets_h_content.find("TopStripItem") != std::string::npos);
    CHECK(widgets_h_content.find("StatusSquare") != std::string::npos);
    CHECK(widgets_h_content.find("void Row(") != std::string::npos);
    CHECK(widgets_h_content.find("RowEnd") != std::string::npos);
    CHECK(widgets_h_content.find("RowList") == std::string::npos);
    CHECK(widgets_h_content.find("StatCard") == std::string::npos);
    CHECK(buttons_content.find("metrics::topstrip_height * scale") != std::string::npos);
    CHECK(display_content.find("metrics::status_square") != std::string::npos);
    CHECK(display_content.find("studio_metrics::row_height * scale") != std::string::npos);
    CHECK(display_content.find("BeginChild") == std::string::npos);
    std::ifstream insights("src/ui/pages/InsightsPage.cpp");
    REQUIRE(static_cast<bool>(insights));
    const std::string insights_content((std::istreambuf_iterator<char>(insights)),
                                       std::istreambuf_iterator<char>());
    CHECK(insights_content.find("HeatPack") == std::string::npos);
    CHECK(insights_content.find("HeatColor(level)") != std::string::npos);
    // T10d: Insights threads the live palette (no Paper read), keeps the
    // HeatColor data-viz fills, and carries no stub or card helper.
    CHECK(insights_content.find("kLightPalette") == std::string::npos);
    CHECK(insights_content.find("Color(palette.bg)") != std::string::npos);
    CHECK(insights_content.find("Pack(palette.border)") != std::string::npos);
    CHECK(insights_content.find("Color(palette.muted)") != std::string::npos);
    CHECK(insights_content.find("Pack(palette.text)") != std::string::npos);
    CHECK(insights_content.find("Pack(palette.accent)") != std::string::npos);
    CHECK(insights_content.find("BeginCard(") == std::string::npos);
    CHECK(insights_content.find("kComingSoon") == std::string::npos);
    CHECK(insights_content.find("kTabVoice") == std::string::npos);
    CHECK(insights_content.find("TabRow") == std::string::npos);
    CHECK(insights_content.find("DrawShareDialog") == std::string::npos);
}

TEST_CASE("Phase 5 pages carry cursor Dummy compensation") {
    // DECISION: each migrated page restores the cursor with
    // "- ImGui::GetStyle().ItemSpacing.y" and then issues an empty
    // Dummy(0,0) so the following row advance stays exact (see
    // docs/PROGRESS.md IMG-001). Pin the pair so a later palette or
    // cursor edit cannot silently drop the compensation.
    const std::vector<std::string> page_sources = {
        "src/ui/pages/DictionaryPage.cpp",
        "src/ui/pages/SnippetsPage.cpp",
        "src/ui/pages/InsightsPage.cpp",
        "src/ui/pages/TransformsPage.cpp",
    };
    for (const std::string& path : page_sources) {
        std::ifstream file(path);
        REQUIRE(static_cast<bool>(file));
        INFO(path);
        const std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        const std::size_t spacing = content.find("- ImGui::GetStyle().ItemSpacing.y");
        REQUIRE(spacing != std::string::npos);
        const std::size_t dummy = content.find("Dummy(ImVec2(0.0f, 0.0f))", spacing);
        CHECK(dummy != std::string::npos);
    }
}

TEST_CASE("Phase 3 widgets use single Palette with Ivory default") {
    const std::vector<std::string> widget_sources = {
        "src/ui/widgets/Buttons.cpp",
        "src/ui/widgets/Display.cpp",
        "src/ui/widgets/Overlays.cpp",
        "src/ui/widgets/Widgets.h",
        "src/ui/widgets/Overlays.h",
    };
    for (const std::string& path : widget_sources) {
        std::ifstream file(path);
        REQUIRE(static_cast<bool>(file));
        INFO(path);
        const std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        CHECK(content.find("StudioPalette") == std::string::npos);
        CHECK(content.find("Pack(kDarkPalette") == std::string::npos);
    }
    std::ifstream widgets_h("src/ui/widgets/Widgets.h");
    REQUIRE(static_cast<bool>(widgets_h));
    const std::string widgets_h_content((std::istreambuf_iterator<char>(widgets_h)),
                                        std::istreambuf_iterator<char>());
    CHECK(widgets_h_content.find("kLightPalette") != std::string::npos);
    std::ifstream buttons("src/ui/widgets/Buttons.cpp");
    REQUIRE(static_cast<bool>(buttons));
    const std::string buttons_content2((std::istreambuf_iterator<char>(buttons)),
                                       std::istreambuf_iterator<char>());
    CHECK(buttons_content2.find("kLightPalette") != std::string::npos);
    std::ifstream display("src/ui/widgets/Display.cpp");
    REQUIRE(static_cast<bool>(display));
    const std::string display_content2((std::istreambuf_iterator<char>(display)),
                                       std::istreambuf_iterator<char>());
    CHECK(display_content2.find("HeatColor(HeatLevel") != std::string::npos);
    CHECK(display_content2.find("- ImGui::GetStyle().ItemSpacing.y") != std::string::npos);
    std::ifstream overlays("src/ui/widgets/Overlays.cpp");
    REQUIRE(static_cast<bool>(overlays));
    const std::string overlays_content2((std::istreambuf_iterator<char>(overlays)),
                                        std::istreambuf_iterator<char>());
    CHECK(overlays_content2.find("Palette& palette") != std::string::npos);
    CHECK(overlays_content2.find("- ImGui::GetStyle().ItemSpacing.y") != std::string::npos);
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

namespace {

// W1-IMPL-03 (T-004 / P-005): literal-bypass ban over the Full-window layout
// sources. ReadSource tries LWF_SOURCE_DIR first with a bare-relative fallback
// (pattern from tests/test_transforms_page.cpp:30-44) so the scan behaves the
// same under ctest (WORKING_DIRECTORY = source dir) and when the exe is
// invoked from build/ — bare ifstream paths return empty elsewhere and would
// mask the CWD trap behind the REQUIRE_FALSE(content.empty()) guard.
std::string ReadSourceFrom(const std::string& root, const std::string& relative) {
    std::ifstream file(root + "/" + relative);
    if (!file.is_open()) {
        file.open(relative);
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

std::string ReadSource(const std::string& relative) {
#ifdef LWF_SOURCE_DIR
    return ReadSourceFrom(LWF_SOURCE_DIR, relative);
#else
    return ReadSourceFrom(".", relative);
#endif
}

// Parses an optionally signed decimal literal starting at pos (leading spaces
// skipped). Returns false when no number starts there (identifier, token
// reference, closing paren). Every matched value here is a short decimal that
// is exactly representable in binary (integers and 0.5), so the exact
// equality checks in ScanLayoutLiterals are safe.
bool ParseNumberAt(const std::string& line, std::size_t pos, double& value) {
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    bool negative = false;
    if (pos < line.size() && line[pos] == '-') {
        negative = true;
        ++pos;
    }
    bool any_digit = false;
    double whole = 0.0;
    while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
        whole = whole * 10.0 + static_cast<double>(line[pos] - '0');
        any_digit = true;
        ++pos;
    }
    double frac = 0.0;
    double divisor = 1.0;
    if (pos < line.size() && line[pos] == '.') {
        ++pos;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
            frac = frac * 10.0 + static_cast<double>(line[pos] - '0');
            divisor *= 10.0;
            any_digit = true;
            ++pos;
        }
    }
    if (!any_digit) {
        return false;
    }
    value = negative ? -(whole + frac / divisor) : (whole + frac / divisor);
    return true;
}

// One token-gap allowance: the (file, API, value) ceiling for digit literals
// that have NO same-value token in either palette (W0-INV-02 §1.4). Any
// literal outside this table, or above its cap, fails the ban. Caps are
// ceilings, not targets: tokenizing a gap site (or hoisting it to a named
// constant) only lowers the count and keeps the test green. Wave 2 T-003
// closed every entry below by adding the 9 studio_metrics tokens and
// file-local hoists; the table ships empty and must stay empty. Do not
// remap a future gap to a nearest-value token (that would shift rendered
// pixels under a green test).
struct BanAllowance {
    const char* file;
    const char* api;
    double value;
    std::size_t cap;
};

const BanAllowance kBanAllowances[] = {};

// Scans one scoped source file for digit literals in the P-005 layout APIs:
// PushFont(nullptr, N), Dummy(ImVec2(0.0f, N)) (height gap), Dummy(ImVec2(N,
// ...)) (width), SameLine(0.0f, N), SetNextItemWidth(N), where N is a numeric
// literal outside the universal geometric factors {0, 0.5, 1, 2} (plus the
// -1.0f fill-width sentinel for SetNextItemWidth). Lines carrying a token
// reference (metrics::/studio_metrics::) are exempt by construction, as are
// identifier / named-constant arguments (e.g. button_gap, kRowButtonGap):
// the lexical shape deliberately leaves named-constant hoisting as the
// spelling-only escape for gap values with no token in this wave. Excluded
// from scope: ThemePalette.cpp (frozen token internals),
// Overlay.cpp (Mini window).
// Generic ImVec2 geometry outside Dummy (PushStyleVar padding, Button
// extents) is not one of the P-005 APIs and is deliberately not scanned.
std::vector<std::string> ScanLayoutLiterals(const std::string& path,
                                            const std::string& content) {
    std::vector<std::string> violations;
    const std::size_t allowance_total =
        sizeof(kBanAllowances) / sizeof(kBanAllowances[0]);
    std::vector<std::size_t> allowance_counts(allowance_total, 0);
    std::size_t begin = 0;
    std::size_t line_no = 1;
    while (begin <= content.size()) {
        const std::size_t end = content.find('\n', begin);
        const std::string line =
            content.substr(begin, end == std::string::npos
                                      ? std::string::npos
                                      : end - begin);
        // Whole-line comments and token-carrying lines are never violations.
        const std::size_t first = line.find_first_not_of(" \t\r");
        const bool comment_line = first != std::string::npos && line[first] == '/' &&
                                  first + 1 < line.size() && line[first + 1] == '/';
        const bool tokened = line.find("metrics::") != std::string::npos ||
                             line.find("studio_metrics::") != std::string::npos;
        if (!comment_line && !tokened) {
            const auto scan_anchor = [&violations, &allowance_counts, &allowance_total,
                                      &line, &line_no, &path](const std::string& anchor,
                                                             const std::string& api) {
                std::size_t from = 0;
                while (true) {
                    const std::size_t hit = line.find(anchor, from);
                    if (hit == std::string::npos) {
                        break;
                    }
                    double value = 0.0;
                    if (ParseNumberAt(line, hit + anchor.size(), value)) {
                        // Universal geometric factors + the width sentinel.
                        const bool universal = value == 0.0 || value == 0.5 ||
                                               value == 1.0 || value == 2.0 ||
                                               (api == "SetNextItemWidth" &&
                                                value == -1.0);
                        if (!universal) {
                            bool matched = false;
                            for (std::size_t i = 0; i < allowance_total; ++i) {
                                const BanAllowance& entry = kBanAllowances[i];
                                if (path == entry.file && api == entry.api &&
                                    value == entry.value) {
                                    allowance_counts[i] += 1;
                                    matched = true;
                                    break;
                                }
                            }
                            if (!matched) {
                                violations.push_back(
                                    path + ":" + std::to_string(line_no) + ": " +
                                    api + " digit literal must use metrics::/" +
                                    "studio_metrics::: " +
                                    line);
                            }
                        }
                    }
                    from = hit + anchor.size();
                }
            };
            scan_anchor("PushFont(nullptr,", "PushFont");
            scan_anchor("Dummy(ImVec2(0.0f,", "DummyHeight");
            scan_anchor("Dummy(ImVec2(", "DummyWidth");
            scan_anchor("SameLine(0.0f,", "SameLine");
            scan_anchor("SetNextItemWidth(", "SetNextItemWidth");
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
        ++line_no;
    }
    for (std::size_t i = 0; i < allowance_total; ++i) {
        const BanAllowance& entry = kBanAllowances[i];
        if (path == entry.file && allowance_counts[i] > entry.cap) {
            violations.push_back(path + ": " + entry.api +
                                 " token-gap allowance exceeded (" +
                                 std::to_string(allowance_counts[i]) + " > cap " +
                                 std::to_string(entry.cap) + ")");
        }
    }
    return violations;
}

}  // namespace

TEST_CASE(
    "full window layout APIs use tokens not digit literals outside the token-gap "
    "allowlist") {
    // P-005 ban (W1-IMPL-03 / W0-INV-03 §5): digit literals in PushFont /
    // Dummy / SameLine / SetNextItemWidth across the scoped Full-window
    // sources must route through metrics::/studio_metrics::, except the
    // capped token-gap allowlist (Wave 2 T-003). Red until T-001/T-002 land;
    // the ban, not a count, is what stops the 37-literal PushFont drift from
    // returning while the page tests stayed green.
    // New Full-window sources must be added here (see exclusions :1169-1171);
    // Windows type-check (G3) runs per-file.
    const std::vector<std::string> layout_sources = {
        "src/ui/MainWindow.cpp",
        "src/ui/pages/DictionaryPage.cpp",
        "src/ui/pages/HistoryPage.cpp",
        "src/ui/pages/ProfilesPage.cpp",
        "src/ui/pages/InsightsPage.cpp",
        "src/ui/pages/SnippetsPage.cpp",
        "src/ui/pages/TransformsPage.cpp",
        "src/ui/widgets/Buttons.cpp",
        "src/ui/widgets/Display.cpp",
        "src/ui/widgets/Overlays.cpp",
    };
    for (const std::string& path : layout_sources) {
        const std::string content = ReadSource(path);
        INFO(path);
        REQUIRE_FALSE(content.empty());
        const std::vector<std::string> violations =
            ScanLayoutLiterals(path, content);
        for (const std::string& violation : violations) {
            INFO(violation);
        }
        CHECK(violations.empty());
    }

    // Brief §3 flip: the studio pages must now push token fonts. The metrics
    // requirement is anchored to the PushFont call in the unqualified or the
    // fully qualified spelling — a loose "metrics::font_" substring would
    // pass on studio_metrics:: alone and prove nothing.
    const auto has_metrics_font = [](const std::string& text) {
        return text.find("PushFont(nullptr, metrics::font_") != std::string::npos ||
               text.find("PushFont(nullptr, lwf::ui::metrics::font_") !=
                   std::string::npos;
    };
    const auto has_studio_font = [](const std::string& text) {
        return text.find("PushFont(nullptr, studio_metrics::font_") !=
                   std::string::npos ||
               text.find("PushFont(nullptr, lwf::ui::studio_metrics::font_") !=
                   std::string::npos;
    };
    const std::string dictionary = ReadSource("src/ui/pages/DictionaryPage.cpp");
    const std::string insights = ReadSource("src/ui/pages/InsightsPage.cpp");
    const std::string snippets = ReadSource("src/ui/pages/SnippetsPage.cpp");
    const std::string transforms = ReadSource("src/ui/pages/TransformsPage.cpp");
    const std::string mainwindow = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(dictionary.empty());
    REQUIRE_FALSE(insights.empty());
    REQUIRE_FALSE(snippets.empty());
    REQUIRE_FALSE(transforms.empty());
    REQUIRE_FALSE(mainwindow.empty());
    INFO("MainWindow.cpp");
    CHECK(has_metrics_font(mainwindow));
    INFO("DictionaryPage.cpp");
    CHECK(has_studio_font(dictionary));
    INFO("InsightsPage.cpp");
    CHECK(has_studio_font(insights));
    INFO("SnippetsPage.cpp");
    CHECK(has_studio_font(snippets));
    INFO("TransformsPage.cpp");
    CHECK(has_studio_font(transforms));
}

TEST_CASE("layout literal ban fires on violations and cap-exceeds") {
    // R-W1-001 negative direction: ScanLayoutLiterals must prove it fires.
    // Synthetic strings only; no real source files are read or edited here.
    // Display.cpp has no PushFont allowance, so a plain digit fires.
    const std::vector<std::string> digit_violation = ScanLayoutLiterals(
        "src/ui/widgets/Display.cpp",
        "ImGui::PushFont(nullptr, 13.0f);\n");
    CHECK_FALSE(digit_violation.empty());

    // Buttons.cpp has no PushFont allowance (table empty); both lines are
    // digit violations.
    const std::vector<std::string> digit_violations = ScanLayoutLiterals(
        "src/ui/widgets/Buttons.cpp",
        "ImGui::PushFont(nullptr, 24.0f);\n"
        "ImGui::PushFont(nullptr, 24.0f);\n");
    CHECK_FALSE(digit_violations.empty());

    // Universal geometric factors never violate.
    const std::vector<std::string> universals = ScanLayoutLiterals(
        "src/ui/widgets/Display.cpp",
        "ImGui::SameLine(0.0f, 0.5f);\n"
        "ImGui::SameLine(0.0f, 1.0f);\n"
        "ImGui::SameLine(0.0f, 2.0f);\n"
        "ImGui::SameLine(0.0f, 0.0f);\n");
    CHECK(universals.empty());

    // The -1.0f sentinel exemption is SetNextItemWidth-only.
    const std::vector<std::string> sentinel_pushfont = ScanLayoutLiterals(
        "src/ui/widgets/Display.cpp",
        "ImGui::PushFont(nullptr, -1.0f);\n");
    CHECK_FALSE(sentinel_pushfont.empty());
    const std::vector<std::string> sentinel_width = ScanLayoutLiterals(
        "src/ui/widgets/Display.cpp",
        "ImGui::SetNextItemWidth(-1.0f);\n");
    CHECK(sentinel_width.empty());

    // Dummy double-anchor safety: DummyWidth sees the 0.0f first component
    // (universal, skipped), DummyHeight sees the 12.0f height. The allowance
    // table is empty, so the literal 12.0f is now a violation.
    const std::vector<std::string> dummy_literal = ScanLayoutLiterals(
        "src/ui/pages/DictionaryPage.cpp",
        "ImGui::Dummy(ImVec2(0.0f, 12.0f));\n");
    CHECK_FALSE(dummy_literal.empty());

    // A tokened gap is genuinely safe (token exemption).
    const std::vector<std::string> dummy_tokened = ScanLayoutLiterals(
        "src/ui/pages/DictionaryPage.cpp",
        "ImGui::Dummy(ImVec2(0.0f, studio_metrics::gap_section * scale));\n");
    CHECK(dummy_tokened.empty());

    // A universal-factor gap is genuinely safe.
    const std::vector<std::string> dummy_universal = ScanLayoutLiterals(
        "src/ui/pages/DictionaryPage.cpp",
        "ImGui::Dummy(ImVec2(0.0f, 1.0f));\n");
    CHECK(dummy_universal.empty());
}

TEST_CASE("api key entry helpers trim, source and labels") {
    // Pure logic for the masked key rows: trim preserves inner bytes, the
    // source prefers a saved key over the environment, labels stay in
    // sentence case, and an empty trimmed value blocks the save.
    CHECK(SettingsUiModel::TrimApiKeyInput("  sk-abc  ") == "sk-abc");
    CHECK(SettingsUiModel::TrimApiKeyInput("\t key \r\n") == "key");
    CHECK(SettingsUiModel::TrimApiKeyInput("   ").empty());
    CHECK(SettingsUiModel::TrimApiKeyInput(" a b ") == "a b");
    CHECK(SettingsUiModel::ApiKeySourceFor(true, true) == SettingsUiModel::ApiKeySource::Saved);
    CHECK(SettingsUiModel::ApiKeySourceFor(true, false) == SettingsUiModel::ApiKeySource::Saved);
    CHECK(SettingsUiModel::ApiKeySourceFor(false, true) ==
          SettingsUiModel::ApiKeySource::Environment);
    CHECK(SettingsUiModel::ApiKeySourceFor(false, false) == SettingsUiModel::ApiKeySource::NotSet);
    CHECK(std::string(SettingsUiModel::ApiKeySourceLabel(SettingsUiModel::ApiKeySource::Saved)) ==
          "Saved key");
    CHECK(std::string(SettingsUiModel::ApiKeySourceLabel(
              SettingsUiModel::ApiKeySource::Environment)) == "Environment key");
    CHECK(std::string(SettingsUiModel::ApiKeySourceLabel(
              SettingsUiModel::ApiKeySource::NotSet)) == "Not set");
    CHECK(std::string(SettingsUiModel::ApiKeyStatusWord(
              SettingsUiModel::ApiKeySource::Saved)) == "Ready");
    CHECK(std::string(SettingsUiModel::ApiKeyStatusWord(
              SettingsUiModel::ApiKeySource::Environment)) == "Ready");
    CHECK(std::string(SettingsUiModel::ApiKeyStatusWord(
              SettingsUiModel::ApiKeySource::NotSet)) == "Not set");
    CHECK(SettingsUiModel::ApiKeySaveError("sk-abc").empty());
    CHECK(SettingsUiModel::ApiKeySaveError("") == "Enter a key to save.");
}

TEST_CASE("api key rows use masked input and named removal") {
    // lwf_tests does not link ImGui, so the row wiring is guarded lexically:
    // masked input, verb-first save, per-service named removal, Esc cancel,
    // focus on Keep, and buffer clear on commit and page switch.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    CHECK(content.find("ImGuiInputTextFlags_Password") != std::string::npos);
    CHECK(content.find("DrawApiKeyRow(\"gnani\"") != std::string::npos);
    CHECK(content.find("##card_cloud_keys") != std::string::npos);
    CHECK(content.find("##card_cleanup_backend") != std::string::npos);
    CHECK(content.find("\"Save key\"") != std::string::npos);
    CHECK(content.find("\"Remove saved key\"") != std::string::npos);
    CHECK(content.find("\"Remove saved Gnani key?\"") != std::string::npos);
    CHECK(content.find("\"Remove Gnani key\"") != std::string::npos);
    CHECK(content.find("\"Remove saved ElevenLabs key?\"") != std::string::npos);
    CHECK(content.find("\"Remove ElevenLabs key\"") != std::string::npos);
    CHECK(content.find("\"Remove saved Muse key?\"") != std::string::npos);
    CHECK(content.find("\"Remove Muse key\"") != std::string::npos);
    CHECK(content.find("\"Remove saved OpenRouter key?\"") != std::string::npos);
    CHECK(content.find("\"Remove OpenRouter key\"") != std::string::npos);
    CHECK(content.find("\"Keep key\"") != std::string::npos);
    CHECK(content.find("ImGuiKey_Escape") != std::string::npos);
    CHECK(content.find("SetKeyboardFocusHere()") != std::string::npos);
    CHECK(content.find("ClearApiKeyInputs();") != std::string::npos);
    CHECK(content.find("Saved keys take precedence over environment variables") !=
          std::string::npos);
    // OR-UI-01 retired pointer: the config-editing hint is gone; the Cleanup
    // card now carries the live model picker. The old literal is asserted
    // ABSENT (not deleted) so restoring it turns this pin red.
    CHECK(content.find("\"The model is set in config.json; see the Privacy tab.\"") ==
          std::string::npos);
    CHECK(content.find("\"Cleanup model\"") != std::string::npos);
    // Stale env-only copy is gone from the Cloud page.
    CHECK(content.find("read from the environment") == std::string::npos);
    // The key value is never seeded into the buffer from the store.
    CHECK(content.find("Seed(") != std::string::npos);
    const std::string row_start = content.substr(content.find("void MainWindow::DrawApiKeyRow"));
    CHECK(row_start.find("store->Get(") == std::string::npos);
    CHECK(row_start.find("store->Set(store_id") != std::string::npos);
    CHECK(row_start.find("store->Remove(store_id)") != std::string::npos);
    // Schema carries no key entry: keys stay in api_keys.json only.
    bool schema_has_api_key = false;
    for (const SettingsStore::SchemaEntry& entry : SettingsStore::Schema()) {
        if (entry.key == "api_key") {
            schema_has_api_key = true;
        }
    }
    CHECK_FALSE(schema_has_api_key);
}

TEST_CASE("microphone picker index maps stored value to the combo entry") {
    // Pure index contract for the Speech-page mic picker: entry 0 is always
    // "System default" (stored value ""), the enumerated device names are
    // offset by one, and a stale/unknown stored name falls back to entry 0.
    const std::vector<std::string> none;
    CHECK(SettingsUiModel::MatchMicDeviceIndex(none, "") == 0);
    CHECK(SettingsUiModel::MatchMicDeviceIndex(none, "Old mic") == 0);
    const std::vector<std::string> names = {"Mic A", "Mic B", "Mic C"};
    CHECK(SettingsUiModel::MatchMicDeviceIndex(names, "") == 0);
    CHECK(SettingsUiModel::MatchMicDeviceIndex(names, "Mic A") == 1);
    CHECK(SettingsUiModel::MatchMicDeviceIndex(names, "Mic B") == 2);
    CHECK(SettingsUiModel::MatchMicDeviceIndex(names, "Mic C") == 3);
    CHECK(SettingsUiModel::MatchMicDeviceIndex(names, "Removed mic") == 0);
    CHECK(SettingsUiModel::MatchMicDeviceIndex(names, "mic a") == 0);
}

TEST_CASE("microphone check verdict thresholds match the engine silent gate") {
    // Followups-3 ("mic is not working"): the check must agree with the
    // engine's silent-mic floor (0.005) so "the check says working" and
    // "dictation says no sound recorded" cannot both happen for the same
    // level. 0.05 marks usable-but-quiet.
    CHECK(SettingsUiModel::MicTestStatusFor(0.0) == SettingsUiModel::MicTestStatus::Silent);
    CHECK(SettingsUiModel::MicTestStatusFor(0.004) == SettingsUiModel::MicTestStatus::Silent);
    CHECK(SettingsUiModel::MicTestStatusFor(0.005) == SettingsUiModel::MicTestStatus::Quiet);
    CHECK(SettingsUiModel::MicTestStatusFor(0.049) == SettingsUiModel::MicTestStatus::Quiet);
    CHECK(SettingsUiModel::MicTestStatusFor(0.05) == SettingsUiModel::MicTestStatus::Good);
    CHECK(SettingsUiModel::MicTestStatusFor(1.0) == SettingsUiModel::MicTestStatus::Good);
    // Every status has non-empty advice and the copy stays level-only.
    for (const SettingsUiModel::MicTestStatus status :
         {SettingsUiModel::MicTestStatus::Good, SettingsUiModel::MicTestStatus::Quiet,
          SettingsUiModel::MicTestStatus::Silent}) {
        const char* advice = SettingsUiModel::MicTestAdvice(status);
        REQUIRE(advice != nullptr);
        CHECK(advice[0] != '\0');
        CHECK(std::string(advice).find('!') == std::string::npos);
    }
}

TEST_CASE("speech page gains the in-app microphone check") {
    // Followups-3: lwf_tests does not link ImGui or open audio devices, so
    // the check is guarded lexically: the card exists on the Speech page
    // under the level meter, the probe runs on a worker thread with a
    // mutex latch (UI thread never blocks), the worker is joined in the
    // destructor, and the probe constructs its own throwaway capture
    // instead of touching the engine's instance.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    CHECK(content.find("MicCheckRow(scale);") != std::string::npos);
    CHECK(content.find("\"##card_mic_check\"") != std::string::npos);
    CHECK(content.find("\"Test microphone##mic_test\"") != std::string::npos);
    CHECK(content.find("MiniaudioCapture probe(16000, 1, device)") != std::string::npos);
    CHECK(content.find("mic_test_worker_ = std::thread") != std::string::npos);
    CHECK(content.find("mic_test_latch_mutex_") != std::string::npos);
    // Placement: after the level meter card, before the Language card.
    const std::size_t level_card = content.find("\"##card_mic_level\"");
    REQUIRE(level_card != std::string::npos);
    const std::size_t check_call = content.find("MicCheckRow(scale);", level_card);
    REQUIRE(check_call != std::string::npos);
    const std::size_t language_card = content.find("\"Language\"", check_call);
    REQUIRE(language_card != std::string::npos);
    CHECK(check_call < language_card);
    // Destructor joins the worker so a test in flight cannot outlive the
    // window (the dtor already joins the models and download workers).
    const std::size_t dtor = content.find("MainWindow::~MainWindow()");
    REQUIRE(dtor != std::string::npos);
    const std::string dtor_span = content.substr(dtor, 2200);
    CHECK(dtor_span.find("mic_test_worker_.joinable()") != std::string::npos);
    CHECK(dtor_span.find("mic_test_worker_.join()") != std::string::npos);
    // Busy guard: the button must not spawn a second probe mid-test and a
    // re-test joins the finished worker instead of overwriting a joinable
    // thread (which would terminate the app).
    const std::size_t row = content.find("void MainWindow::MicCheckRow(float scale)");
    REQUIRE(row != std::string::npos);
    const std::string row_span = content.substr(row, 5200);
    CHECK(row_span.find("mic_test_busy_.exchange(true)") != std::string::npos);
    CHECK(row_span.find("mic_test_worker_.joinable()") != std::string::npos);
    CHECK(row_span.find("ImGui::BeginDisabled()") != std::string::npos);
    // The verdict comes from the shared pure model, not ad-hoc thresholds.
    CHECK(row_span.find("SettingsUiModel::MicTestStatusFor(mic_test_peak_)") != std::string::npos);
    CHECK(row_span.find("SettingsUiModel::MicTestAdvice(status)") != std::string::npos);
}

TEST_CASE("speech page microphone picker replaces the dead free-text row") {
    // lwf_tests does not link ImGui, so the picker wiring is guarded
    // lexically: the card sits between the model and level cards on the
    // Speech page, selection persists through mic_device with the restart
    // banner (schema hot_apply=false), the device list is enumerated once
    // per run, and the removed General free-text row stays gone.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    CHECK(content.find("##card_mic_device") != std::string::npos);
    CHECK(content.find("##mic_pick") != std::string::npos);
    CHECK(content.find("ListCaptureDevices()") != std::string::npos);
    CHECK(content.find("MatchMicDeviceIndex") != std::string::npos);
    CHECK(content.find("mic_devices_loaded_ = true") != std::string::npos);
    // T5 ruled rows: the former BeginCard ids now label the first Row of
    // each section; order intent is model rows < mic picker row < meter row.
    const std::size_t model_card = content.find("\"##card_model\"");
    REQUIRE(model_card != std::string::npos);
    const std::size_t mic_card = content.find("\"##card_mic_device\"", model_card);
    REQUIRE(mic_card != std::string::npos);
    const std::size_t level_card = content.find("\"##card_mic_level\"", mic_card);
    REQUIRE(level_card != std::string::npos);
    CHECK(model_card < mic_card);
    CHECK(mic_card < level_card);
    CHECK(content.find("BeginCard(\"##card_model\"") == std::string::npos);
    CHECK(content.find("BeginCard(\"##card_mic_device\"") == std::string::npos);
    CHECK(content.find("BeginCard(\"##card_mic_level\"") == std::string::npos);
    // Persist + restart banner ordering, mirroring the ##llm_provider pin.
    const std::size_t mic_set = content.find("SetString(\"mic_device\"");
    REQUIRE(mic_set != std::string::npos);
    const std::size_t mic_apply = content.find("ApplyAndSave()", mic_set);
    REQUIRE(mic_apply != std::string::npos);
    CHECK(content.find("restart_needed_ = true", mic_set) < mic_apply);
    // Strengthened: EVERY select path of the ##mic_pick combo ("System
    // default" with value "" plus the named-device loop) must set, flag the
    // restart, then save. A mutation dropping `restart_needed_ = true` from
    // one path must go red, so iterate all occurrences and pin the
    // set -> restart -> apply order per path, close enough that the flag and
    // the save cannot be picked up from a later, unrelated settings card.
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    const std::string mic_set_needle = "SetString(\"mic_device\"";
    const std::size_t mic_paths = count_occurrences(content, mic_set_needle);
    REQUIRE(mic_paths >= 2);  // System default + named device.
    std::size_t path_set = content.find(mic_set_needle);
    for (std::size_t path = 0; path < mic_paths; ++path) {
        REQUIRE(path_set != std::string::npos);
        const std::size_t path_restart = content.find("restart_needed_ = true", path_set);
        REQUIRE(path_restart != std::string::npos);
        REQUIRE(path_restart - path_set < 200);  // Same select branch.
        const std::size_t path_apply = content.find("ApplyAndSave()", path_restart);
        REQUIRE(path_apply != std::string::npos);
        REQUIRE(path_apply - path_restart < 200);  // Same select branch.
        path_set = content.find(mic_set_needle, path_set + mic_set_needle.size());
    }
    // Lazy-latch pin: the literal `mic_devices_loaded_ = true` alone is not
    // enough — a mutation enumerating every frame could keep it. Enumeration
    // must sit directly inside the `if (!mic_devices_loaded_)` guard (only
    // the DECISION comment between guard and call), before the latch is set,
    // with exactly one call site in the translation unit.
    CHECK(count_occurrences(content, "ListCaptureDevices()") == 1);
    const std::size_t latch_guard = content.find("if (!mic_devices_loaded_)");
    REQUIRE(latch_guard != std::string::npos);
    const std::size_t list_call = content.find("ListCaptureDevices()", latch_guard);
    REQUIRE(list_call != std::string::npos);
    const std::size_t prev_newline = content.rfind('\n', list_call);
    REQUIRE(prev_newline != std::string::npos);
    const std::size_t call_line_start = prev_newline + 1;
    const std::size_t guard_line_end = content.find('\n', latch_guard);
    REQUIRE(guard_line_end != std::string::npos);
    REQUIRE(guard_line_end < call_line_start);
    // Everything between the guard line and the call line must be comments:
    // closing the latch first and enumerating after it breaks the pin.
    bool comments_only_between = true;
    for (std::size_t scan = guard_line_end + 1; scan < call_line_start;) {
        const std::size_t eol = content.find('\n', scan);
        const std::size_t line_end =
            (eol == std::string::npos || eol > call_line_start) ? call_line_start : eol;
        const std::string line = content.substr(scan, line_end - scan);
        const std::size_t first = line.find_first_not_of(" \t\r");
        if (first != std::string::npos && line.compare(first, 2, "//") != 0) {
            comments_only_between = false;
            break;
        }
        scan = line_end + 1;
    }
    CHECK(comments_only_between);
    const std::size_t latch_set = content.find("mic_devices_loaded_ = true", list_call);
    REQUIRE(latch_set != std::string::npos);
    CHECK(latch_set - list_call < 300);
    // Dead plumbing removed: no free-text row and no buffer member.
    CHECK(content.find("StringFieldRow(\"mic_device\"") == std::string::npos);
    const std::string header = ReadSource("src/ui/MainWindow.h");
    REQUIRE_FALSE(header.empty());
    CHECK(header.find("mic_device_field_") == std::string::npos);
}

TEST_CASE("T5 settings General and Speech ruled rows pin") {
    // T5-owned pins: theme switcher writes the canonical constants, load-time
    // normalization lives in Init, both pages use ruled rows (widgets::Row +
    // widgets::ToggleSwitch with the live palette), and the legacy
    // hotkey_code row removed at T14 (schema key stays).
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    const std::size_t general = content.find("void MainWindow::DrawGeneralPage");
    REQUIRE(general != std::string::npos);
    const std::size_t settings_fn = content.find("void MainWindow::DrawSettingsPage", general);
    REQUIRE(settings_fn != std::string::npos);
    const std::string general_src = content.substr(general, settings_fn - general);
    const std::size_t speech = content.find("void MainWindow::DrawSpeechPage");
    REQUIRE(speech != std::string::npos);
    const std::size_t cloud_fn = content.find("void MainWindow::DrawCloudPage", speech);
    REQUIRE(cloud_fn != std::string::npos);
    const std::string speech_src = content.substr(speech, cloud_fn - speech);
    // Theme combo: display Graphite/Paper, write the exact constants.
    CHECK(general_src.find("kThemeGraphite") != std::string::npos);
    CHECK(general_src.find("kThemePaper") != std::string::npos);
    CHECK(general_src.find("SetString(\"theme\"") != std::string::npos);
    CHECK(general_src.find("\"Graphite\"") != std::string::npos);
    CHECK(general_src.find("\"Paper\"") != std::string::npos);
    CHECK(general_src.find("\"##theme\"") != std::string::npos);
    // Load-time normalization in Init: unknown -> graphite + save.
    const std::size_t init_fn = content.find("bool MainWindow::Init()");
    REQUIRE(init_fn != std::string::npos);
    const std::size_t shutdown_fn = content.find("void MainWindow::Shutdown", init_fn);
    REQUIRE(shutdown_fn != std::string::npos);
    const std::string init_src = content.substr(init_fn, shutdown_fn - init_fn);
    CHECK(init_src.find("kThemeGraphite") != std::string::npos);
    CHECK(init_src.find("kThemePaper") != std::string::npos);
    CHECK(init_src.find("SetString(\"theme\"") != std::string::npos);
    CHECK(init_src.find("settings_.Save()") != std::string::npos);
    // Ruled rows with the live palette (never the Paper default).
    CHECK(general_src.find("widgets::Row(") != std::string::npos);
    CHECK(general_src.find("*current_palette_") != std::string::npos);
    CHECK(speech_src.find("widgets::Row(") != std::string::npos);
    CHECK(speech_src.find("*current_palette_") != std::string::npos);
    CHECK(general_src.find("widgets::ToggleSwitch(") != std::string::npos);
    CHECK(speech_src.find("widgets::ToggleSwitch(") != std::string::npos);
    // legacy row removed at T14 (schema key stays).
    CHECK(general_src.find("IntRow(\"hotkey_code\"") == std::string::npos);
    CHECK(general_src.find("##hotkey_code") == std::string::npos);
    // Restart banner: kept on Speech, added to General.
    CHECK(general_src.find("DrawRestartBanner(scale)") != std::string::npos);
    CHECK(speech_src.find("DrawRestartBanner(scale)") != std::string::npos);
    // These two tabs no longer call BeginCard (the def stays for T6-T8).
    CHECK(general_src.find("BeginCard(") == std::string::npos);
    CHECK(speech_src.find("BeginCard(") == std::string::npos);
    CHECK(content.find("void MainWindow::BeginCard") != std::string::npos);
    // T5 toggles use the widget switch directly (ToggleRow/BoolRow stay).
    CHECK(general_src.find("ToggleRow(") == std::string::npos);
    CHECK(speech_src.find("ToggleRow(") == std::string::npos);
    CHECK(general_src.find("BoolRow(") == std::string::npos);
    CHECK(speech_src.find("BoolRow(") == std::string::npos);
    // T5-fix err/warn palette (live palette, never Paper): the T5 tabs sit
    // directly on the window bg, so Paper err/warn read at 2.54/2.80:1 there
    // (T5 reviewer, MAJOR). Scoped to each T5 function span (T4-fix
    // StatusSquare style) so a Cloud/Cleanup/Privacy/Home Paper lookalike
    // cannot satisfy the pin; the absence check pins the Paper variant out.
    const auto count_occurrences_t5 = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    const std::size_t banner_fn = content.find("void MainWindow::DrawRestartBanner");
    REQUIRE(banner_fn != std::string::npos);
    const std::size_t chips_fn = content.find("void MainWindow::DrawHotkeyChips", banner_fn);
    REQUIRE(chips_fn != std::string::npos);
    const std::string banner_src = content.substr(banner_fn, chips_fn - banner_fn);
    CHECK(banner_src.find("Color(current_palette_->warn") != std::string::npos);
    CHECK(banner_src.find("Color(kLightPalette.warn") == std::string::npos);
    const std::size_t hotkey_row_fn = content.find("void MainWindow::DrawGeneralHotkeyRow");
    REQUIRE(hotkey_row_fn != std::string::npos);
    const std::size_t home_fn = content.find("void MainWindow::DrawHomePage", hotkey_row_fn);
    REQUIRE(home_fn != std::string::npos);
    const std::string hotkey_row_src = content.substr(hotkey_row_fn, home_fn - hotkey_row_fn);
    CHECK(count_occurrences_t5(hotkey_row_src, "Color(current_palette_->err") >= 2);
    CHECK(hotkey_row_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(count_occurrences_t5(speech_src, "Color(current_palette_->err") >= 2);
    CHECK(speech_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(general_src.find("Color(current_palette_->err") != std::string::npos);
    CHECK(general_src.find("Color(kLightPalette.err") == std::string::npos);
    // Strengthened (T5-fix): needle the ACTUAL SetString call sites, not
    // bare constant names. The combo comment ("must be exactly
    // kThemeGraphite/kThemePaper") and the `theme == kThemePaper` display
    // read would otherwise satisfy a bare find even if the writes became
    // string literals. Scope after `##theme` so the pre-combo comment
    // cannot satisfy the write pins, and pin the Init normalize target
    // exactly (a graphite->paper swap must fail, not hide behind the
    // guard comparison).
    const std::size_t theme_combo = general_src.find("\"##theme\"");
    REQUIRE(theme_combo != std::string::npos);
    const std::string theme_writes = general_src.substr(theme_combo);
    CHECK(theme_writes.find("SetString(\"theme\", lwf::ui::kThemeGraphite") !=
          std::string::npos);
    CHECK(theme_writes.find("SetString(\"theme\", lwf::ui::kThemePaper") != std::string::npos);
    CHECK(init_src.find("SetString(\"theme\", lwf::ui::kThemeGraphite") != std::string::npos);
    CHECK(init_src.find("SetString(\"theme\", lwf::ui::kThemePaper") == std::string::npos);
    // Hot-apply: the theme row is hot_apply, so no restart flag may be set
    // between its Row (`##card_general_window`) and its RowEnd (span-scoped
    // absence, mic-pin style).
    const std::size_t theme_row = general_src.find("\"##card_general_window\"");
    REQUIRE(theme_row != std::string::npos);
    const std::size_t theme_row_end = general_src.find("RowEnd(scale)", theme_row);
    REQUIRE(theme_row_end != std::string::npos);
    const std::string theme_span = general_src.substr(theme_row, theme_row_end - theme_row);
    CHECK(theme_span.find("restart_needed_ = true") == std::string::npos);
}

TEST_CASE("T13a speech model download wiring pin") {
    // The catalog-driven Download control lives in DrawSpeechPage only:
    // worker thread, cancel flag, latch, empty-url disabled state, and the
    // ErrorCode mapping. The transfer layer itself stays in os/ (T13a adds
    // no network code), and Speech is the only page that spawns it.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    const std::size_t speech = content.find("void MainWindow::DrawSpeechPage");
    REQUIRE(speech != std::string::npos);
    const std::size_t cloud_fn = content.find("void MainWindow::DrawCloudPage", speech);
    REQUIRE(cloud_fn != std::string::npos);
    const std::string speech_src = content.substr(speech, cloud_fn - speech);
    CHECK(speech_src.find("ParseModelsJson") != std::string::npos);
    CHECK(speech_src.find("BeginDisabled(") != std::string::npos);
    CHECK(speech_src.find("\"Download##download_speech\"") != std::string::npos);
    CHECK(speech_src.find("\"Cancel##cancel_download\"") != std::string::npos);
    CHECK(speech_src.find("download_busy_.load()") != std::string::npos);
    CHECK(speech_src.find("download_cancelled_.store(true)") != std::string::npos);
    CHECK(speech_src.find("IsSafeFileName") != std::string::npos);
    CHECK(speech_src.find("PlanDownloadStart") != std::string::npos);
    CHECK(speech_src.find("GetDataDir()") != std::string::npos);
    // The latch is consumed in RunOnce (before the hidden early-out), not
    // on the Speech page, so completion survives a hidden window.
    const std::size_t run_once = content.find("void MainWindow::RunOnce");
    REQUIRE(run_once != std::string::npos);
    const std::size_t run_once_end = content.find("void MainWindow::RenderPages", run_once);
    REQUIRE(run_once_end != std::string::npos);
    const std::string run_once_src = content.substr(run_once, run_once_end - run_once);
    CHECK(run_once_src.find("download_latch_mutex_") != std::string::npos);
    CHECK(run_once_src.find("download_model_.OnSuccess()") != std::string::npos);
    CHECK(run_once_src.find("download_model_.OnFailed(") != std::string::npos);
    CHECK(run_once_src.find("download_model_.OnCancelled()") != std::string::npos);
    // The worker owns the synchronous transfer and is joined in the
    // destructor after the cancel flag is set; the worker never touches
    // ImGui. The ErrorCode mapping stays in the portable model.
    CHECK(content.find("WindowsDownloader downloader") != std::string::npos);
    CHECK(content.find("MapDownloadError") != std::string::npos);
    CHECK(content.find("IsCancellationDetail") != std::string::npos);
    const std::size_t dtor = content.find("MainWindow::~MainWindow()");
    REQUIRE(dtor != std::string::npos);
    const std::size_t init_fn = content.find("bool MainWindow::Init()", dtor);
    REQUIRE(init_fn != std::string::npos);
    const std::string dtor_src = content.substr(dtor, init_fn - dtor);
    CHECK(dtor_src.find("download_worker_.join()") != std::string::npos);
    const std::string onboarding = ReadSource("src/core/Onboarding.cpp");
    REQUIRE_FALSE(onboarding.empty());
    CHECK(onboarding.find("ErrorCode::DownloadFailed") != std::string::npos);
    CHECK(onboarding.find("ErrorCode::ChecksumMismatch") != std::string::npos);
    // Zero new network code: no OS network headers in the UI translation
    // unit, and the transfer header (cstdint/functional/optional/string
    // only) is the only network-adjacent include added.
    CHECK(content.find("windows.h") == std::string::npos);
    CHECK(content.find("winhttp.h") == std::string::npos);
    CHECK(content.find("os/WindowsDownloader.h") != std::string::npos);
}

TEST_CASE("cleanup model index maps stored value to the fetched list") {
    // Pure index contract for the Cleanup-page model picker: there is no
    // sentinel entry, so "" and not-found both return -1, and the comparison
    // is case-sensitive.
    const std::vector<std::string> none;
    CHECK(SettingsUiModel::MatchModelIndex(none, "") == -1);
    CHECK(SettingsUiModel::MatchModelIndex(none, "some/model") == -1);
    const std::vector<std::string> fetched = {"a/model", "b/model", "c/model"};
    CHECK(SettingsUiModel::MatchModelIndex(fetched, "") == -1);
    CHECK(SettingsUiModel::MatchModelIndex(fetched, "a/model") == 0);
    CHECK(SettingsUiModel::MatchModelIndex(fetched, "b/model") == 1);
    CHECK(SettingsUiModel::MatchModelIndex(fetched, "c/model") == 2);
    CHECK(SettingsUiModel::MatchModelIndex(fetched, "z/model") == -1);
    CHECK(SettingsUiModel::MatchModelIndex(fetched, "A/model") == -1);
    const std::vector<std::string> cased = {"gpt-x"};
    CHECK(SettingsUiModel::MatchModelIndex(cased, "GPT-x") == -1);
}

TEST_CASE("cleanup card OpenRouter model picker wires refresh and persistence") {
    // lwf_tests does not link ImGui, so the picker wiring is guarded
    // lexically: the combo sits in the backend card, Refresh is the only
    // network trigger (worker + latch, never on the UI thread), busy guard
    // before the spawn, and every select path persists through
    // openrouter_model with the restart banner (schema hot_apply=false).
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    CHECK(content.find("##card_cleanup_backend") != std::string::npos);
    CHECK(content.find("##openrouter_model") != std::string::npos);
    CHECK(content.find("\"Refresh model list\"") != std::string::npos);
    CHECK(content.find("\"Cleanup model\"") != std::string::npos);
    CHECK(content.find("\"Type a model id\"") != std::string::npos);
    CHECK(content.find("\"Use this model\"") != std::string::npos);
    CHECK(content.find("MatchModelIndex") != std::string::npos);
    CHECK(content.find("##or_custom") != std::string::npos);
    CHECK(content.find("##or_") != std::string::npos);
    CHECK(content.find("edit config.json") == std::string::npos);
    CHECK(content.find("\"not set\"") != std::string::npos);
    const std::size_t backend = content.find("##card_cleanup_backend");
    REQUIRE(backend != std::string::npos);
    CHECK(content.find("##openrouter_model", backend) != std::string::npos);
    // The fetch call lives in main.cpp, not here: the F9 copy lint scans
    // every literal in src/ui (including #include paths) for banned terms,
    // and the fetcher's header sits under the banned "llm" tree, so the
    // composition root maps the Task-A result onto UI-facing primitives and
    // injects it (see SetModelsFetcher). The off-thread worker call itself
    // stays in MainWindow.cpp through `models_fetcher_`.
    const std::string main_content = ReadSource("src/main.cpp");
    REQUIRE_FALSE(main_content.empty());
    CHECK(main_content.find("OpenRouterModels::FetchOpenRouterModels") !=
          std::string::npos);
    CHECK(content.find("models_fetcher_") != std::string::npos);
    // Persist + restart banner ordering, mirroring the ##llm_provider pin,
    // hardened per occurrence like the ##mic_pick pin: EVERY select path
    // (the combo loop and the manual Use-this-model commit) must set, flag
    // the restart, then save, close enough that the flag and the save cannot
    // be picked up from a later, unrelated settings card.
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    const std::string model_set_needle = "SetString(\"openrouter_model\"";
    const std::size_t model_paths = count_occurrences(content, model_set_needle);
    REQUIRE(model_paths >= 2);  // Combo loop + manual fallback.
    std::size_t path_set = content.find(model_set_needle);
    for (std::size_t path = 0; path < model_paths; ++path) {
        REQUIRE(path_set != std::string::npos);
        const std::size_t path_restart = content.find("restart_needed_ = true", path_set);
        REQUIRE(path_restart != std::string::npos);
        REQUIRE(path_restart - path_set < 200);  // Same select branch.
        const std::size_t path_apply = content.find("ApplyAndSave()", path_restart);
        REQUIRE(path_apply != std::string::npos);
        REQUIRE(path_apply - path_restart < 200);  // Same select branch.
        path_set = content.find(model_set_needle, path_set + model_set_needle.size());
    }
    // Busy-guard pin: the guard must sit between the Refresh button and the
    // worker spawn, close enough that the spawn cannot be reached without it.
    // (Removing the guard also removes the anchor, so this goes red.)
    const std::size_t refresh_button = content.find("\"Refresh model list\"");
    REQUIRE(refresh_button != std::string::npos);
    const std::size_t busy_guard = content.find("if (openrouter_models_busy_", refresh_button);
    REQUIRE(busy_guard != std::string::npos);
    const std::size_t spawn = content.find("openrouter_models_worker_ =", busy_guard);
    REQUIRE(spawn != std::string::npos);
    // The whole Refresh handler (null check + key resolution + join +
    // worker spawn) is about a kilobyte, so the guard-to-spawn window is
    // 1000: close enough that the spawn cannot leave the Refresh handler.
    CHECK(spawn - busy_guard < 1000);
    // Latch-consumption pin: the worker writes the latch, the card draw reads
    // it on the UI thread (busy cleared on consume).
    const std::size_t latch = content.find("if (openrouter_models_latch_ready_)");
    REQUIRE(latch != std::string::npos);
    CHECK(content.find("openrouter_models_latch_.", latch) != std::string::npos);
    CHECK(content.find("openrouter_models_busy_.store(false)") != std::string::npos);
    const std::string header = ReadSource("src/ui/MainWindow.h");
    REQUIRE_FALSE(header.empty());
    CHECK(header.find("SetModelsTransport") != std::string::npos);
    CHECK(header.find("openrouter_models_busy_") != std::string::npos);
    CHECK(header.find("openrouter_model_field_") != std::string::npos);
    CHECK(header.find("openrouter_models_worker_") != std::string::npos);
}

TEST_CASE("T6 settings Cloud and Cleanup ruled rows pin") {
    // T6-owned pins: the two tabs use ruled rows (widgets::Row +
    // widgets::ToggleSwitch with the live palette), keep or carry their
    // former card ids, keep the forced restart (stt_provider, consent and
    // llm_provider spans) and the hot-apply absence (snippet_mode span),
    // and keep the F1 live-palette fixes for err/warn plus the API-key
    // chrome (dot, focus rect, KeyButton). The key-row + picker blocks
    // stay uncarded (documented block exception: taller than the 52 px
    // ruled row) and keep the literal OPENROUTER_API_KEY + lazy entry
    // build. Span-scoped like the T5 case so a Home/Privacy/About Paper
    // read or a lookalike string cannot satisfy the pins by accident.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    const std::size_t cloud = content.find("void MainWindow::DrawCloudPage");
    REQUIRE(cloud != std::string::npos);
    const std::size_t cleanup = content.find("void MainWindow::DrawCleanupPage", cloud);
    REQUIRE(cleanup != std::string::npos);
    const std::size_t privacy = content.find("void MainWindow::DrawPrivacyPage", cleanup);
    REQUIRE(privacy != std::string::npos);
    const std::string cloud_src = content.substr(cloud, cleanup - cloud);
    const std::string cleanup_src = content.substr(cleanup, privacy - cleanup);
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // Ruled rows with the live palette (never the Paper default).
    CHECK(cloud_src.find("widgets::Row(") != std::string::npos);
    CHECK(cloud_src.find("*current_palette_") != std::string::npos);
    CHECK(cleanup_src.find("widgets::Row(") != std::string::npos);
    CHECK(cleanup_src.find("*current_palette_") != std::string::npos);
    // T6 toggles use the widget switch directly (local ToggleRow/BoolRow stay).
    CHECK(cloud_src.find("widgets::ToggleSwitch(") != std::string::npos);
    CHECK(cleanup_src.find("widgets::ToggleSwitch(") != std::string::npos);
    CHECK(cloud_src.find("ToggleRow(") == std::string::npos);
    CHECK(cloud_src.find("BoolRow(") == std::string::npos);
    CHECK(cleanup_src.find("ToggleRow(") == std::string::npos);
    CHECK(cleanup_src.find("BoolRow(") == std::string::npos);
    // These two tabs no longer call BeginCard (the def stays for T7-T8).
    CHECK(cloud_src.find("BeginCard(") == std::string::npos);
    CHECK(cleanup_src.find("BeginCard(") == std::string::npos);
    CHECK(content.find("void MainWindow::BeginCard") != std::string::npos);
    // Restart banner: kept on both pages, not duplicated.
    CHECK(count_occurrences(cloud_src, "DrawRestartBanner(scale)") == 1);
    CHECK(count_occurrences(cleanup_src, "DrawRestartBanner(scale)") == 1);
    // Former card ids: rows carry them directly; the key block carries
    // ##card_cloud_keys via a real PushID (a comment alone must not pass).
    CHECK(cloud_src.find("\"##card_cloud_provider\"") != std::string::npos);
    CHECK(cloud_src.find("ImGui::PushID(\"##card_cloud_keys\")") != std::string::npos);
    CHECK(cloud_src.find("\"##card_remote_engine\"") != std::string::npos);
    CHECK(cloud_src.find("\"##card_cloud_consent\"") != std::string::npos);
    CHECK(cleanup_src.find("\"##card_cleanup\"") != std::string::npos);
    CHECK(cleanup_src.find("\"##card_cleanup_backend\"") != std::string::npos);
    // Slider/combo slots are 248 design px, not 240 (substring pin).
    CHECK(cloud_src.find("248.0f * scale") != std::string::npos);
    CHECK(cleanup_src.find("248.0f * scale") != std::string::npos);
    CHECK(cloud_src.find("240.0f * scale") == std::string::npos);
    CHECK(cleanup_src.find("240.0f * scale") == std::string::npos);
    // Forced restart kept: each span must set the flag before its RowEnd.
    const auto span_has_restart = [](const std::string& src, const std::string& row_id) {
        const std::size_t row = src.find(row_id);
        if (row == std::string::npos) {
            return false;
        }
        const std::size_t row_end = src.find("RowEnd(scale)", row);
        if (row_end == std::string::npos) {
            return false;
        }
        return src.find("restart_needed_ = true", row) < row_end;
    };
    CHECK(span_has_restart(cloud_src, "\"##card_cloud_provider\""));
    CHECK(span_has_restart(cloud_src, "\"##card_cloud_consent\""));
    CHECK(span_has_restart(cleanup_src, "\"##card_cleanup_backend\""));
    // Consent warn gate kept: active only when a non-local service is set.
    CHECK(cloud_src.find("settings_.GetString(\"stt_provider\") != \"local\"") !=
          std::string::npos);
    // Hot-apply absence: snippet_mode is hot_apply=true with no force, so
    // its span must not set the flag.
    {
        const std::size_t row = cleanup_src.find("\"##row_snippet_mode\"");
        REQUIRE(row != std::string::npos);
        const std::size_t row_end = cleanup_src.find("RowEnd(scale)", row);
        REQUIRE(row_end != std::string::npos);
        const std::string span = cleanup_src.substr(row, row_end - row);
        CHECK(span.find("restart_needed_ = true") == std::string::npos);
    }
    // F1 err/warn in the live palette, Paper variant pinned out of both
    // spans. Cloud also draws the validation error after the row block.
    CHECK(count_occurrences(cloud_src, "Color(current_palette_->err") >= 1);
    CHECK(count_occurrences(cloud_src, "Color(current_palette_->warn") >= 1);
    CHECK(count_occurrences(cleanup_src, "Color(current_palette_->err") >= 1);
    CHECK(count_occurrences(cleanup_src, "Color(current_palette_->warn") >= 1);
    CHECK(cloud_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(cloud_src.find("Color(kLightPalette.warn") == std::string::npos);
    CHECK(cleanup_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(cleanup_src.find("Color(kLightPalette.warn") == std::string::npos);
    // API-key row chrome in the live palette: 3 error sites + dot + focus.
    const std::size_t key_row = content.find("void MainWindow::DrawApiKeyRow");
    REQUIRE(key_row != std::string::npos);
    const std::size_t persist_fn = content.find("void MainWindow::PersistPage", key_row);
    REQUIRE(persist_fn != std::string::npos);
    const std::string key_row_src = content.substr(key_row, persist_fn - key_row);
    CHECK(count_occurrences(key_row_src, "Color(current_palette_->err") >= 3);
    CHECK(key_row_src.find("current_palette_->ok") != std::string::npos);
    CHECK(key_row_src.find("current_palette_->muted") != std::string::npos);
    CHECK(key_row_src.find("current_palette_->focus") != std::string::npos);
    CHECK(key_row_src.find("kLightPalette") == std::string::npos);
    // KeyButton internals in the passed palette (button chrome + focus).
    const std::size_t key_button = content.find("bool KeyButton(");
    REQUIRE(key_button != std::string::npos);
    const std::size_t key_button_end = content.find("}  // namespace", key_button);
    REQUIRE(key_button_end != std::string::npos);
    const std::string key_button_src = content.substr(key_button, key_button_end - key_button);
    CHECK(key_button_src.find("palette.accent") != std::string::npos);
    CHECK(key_button_src.find("palette.on_accent") != std::string::npos);
    CHECK(key_button_src.find("palette.focus") != std::string::npos);
    CHECK(key_button_src.find("kLightPalette") == std::string::npos);
    // Block exception + picker identity: the backend key row keeps the
    // literal env name, and the lazy entry build comment survives.
    CHECK(cleanup_src.find("BLOCK EXCEPTION") != std::string::npos);
    CHECK(cloud_src.find("BLOCK EXCEPTION") != std::string::npos);
    CHECK(cleanup_src.find("\"OPENROUTER_API_KEY\"") != std::string::npos);
    CHECK(cleanup_src.find("build the entry list lazily, only when the popup") !=
          std::string::npos);
    CHECK(cleanup_src.find("DrawApiKeyRow(\"openrouter\"") != std::string::npos);
    const std::size_t backend = cleanup_src.find("\"##card_cleanup_backend\"");
    REQUIRE(backend != std::string::npos);
    CHECK(cleanup_src.find("##openrouter_model", backend) != std::string::npos);
}

TEST_CASE("T7 settings Privacy and About ruled rows pin") {
    // T7-owned pins: the two tabs use ruled rows (widgets::Row +
    // widgets::ToggleSwitch with the live palette), carry their former card
    // ids, add the session restart banner, keep the retention keys hot-apply
    // with exact IntRow ranges/formats, upgrade the delete-all confirm to
    // Esc + focus-on-Cancel, clear the delete arm on tab switch, and add the
    // (NEW) read-only shortcuts table on About with zero writes.
    // Span-scoped like the T5/T6 cases so a Home/Cloud Paper read or a
    // lookalike string cannot satisfy the pins by accident.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    const std::size_t privacy = content.find("void MainWindow::DrawPrivacyPage");
    REQUIRE(privacy != std::string::npos);
    const std::size_t about = content.find("void MainWindow::DrawAboutPage", privacy);
    REQUIRE(about != std::string::npos);
    const std::size_t insights = content.find("void MainWindow::DrawInsightsPage", about);
    REQUIRE(insights != std::string::npos);
    const std::string privacy_src = content.substr(privacy, about - privacy);
    const std::string about_src = content.substr(about, insights - about);
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // Function order (name + sequence) preserved: the T6 span delimiter at
    // the Privacy fn must stay green.
    CHECK(privacy < about);
    CHECK(about < insights);
    // Ruled rows with the live palette (never the Paper default).
    CHECK(privacy_src.find("widgets::Row(") != std::string::npos);
    CHECK(privacy_src.find("*current_palette_") != std::string::npos);
    CHECK(privacy_src.find("widgets::ToggleSwitch(") != std::string::npos);
    CHECK(about_src.find("widgets::Row(") != std::string::npos);
    CHECK(about_src.find("*current_palette_") != std::string::npos);
    // T7 toggles use the widget switch directly (local ToggleRow/BoolRow stay).
    CHECK(privacy_src.find("ToggleRow(") == std::string::npos);
    CHECK(privacy_src.find("BoolRow(") == std::string::npos);
    // These two tabs no longer call BeginCard (the def stays for T8).
    CHECK(privacy_src.find("BeginCard(") == std::string::npos);
    CHECK(about_src.find("BeginCard(") == std::string::npos);
    CHECK(content.find("void MainWindow::BeginCard") != std::string::npos);
    // Restart banner: added to both pages, not duplicated (banner reflects
    // the session flag; all Privacy/About keys are hot-apply/read-only).
    CHECK(count_occurrences(privacy_src, "DrawRestartBanner(scale)") == 1);
    CHECK(count_occurrences(about_src, "DrawRestartBanner(scale)") == 1);
    // Former card ids: rows carry them directly; the delete block carries
    // ##card_delete_data via a real PushID.
    CHECK(privacy_src.find("\"##card_privacy\"") != std::string::npos);
    CHECK(privacy_src.find("ImGui::PushID(\"##card_delete_data\")") != std::string::npos);
    CHECK(about_src.find("\"##card_about\"") != std::string::npos);
    // Slider/int slots are 248 design px, not 240 (substring pin).
    CHECK(privacy_src.find("248.0f * scale") != std::string::npos);
    CHECK(about_src.find("248.0f * scale") != std::string::npos);
    CHECK(privacy_src.find("240.0f * scale") == std::string::npos);
    CHECK(about_src.find("240.0f * scale") == std::string::npos);
    // Hot-apply absence: every key on these tabs is hot-apply or read-only,
    // so neither span may set the restart flag (mic-pin style absence).
    CHECK(privacy_src.find("restart_needed_ = true") == std::string::npos);
    CHECK(about_src.find("restart_needed_ = true") == std::string::npos);
    // F1 err/warn in the live palette, Paper variant pinned out of both
    // spans (Privacy err: debug warning + validation error + delete
    // confirm; Privacy warn: OpenRouter readout; About warn: OpenRouter).
    CHECK(count_occurrences(privacy_src, "Color(current_palette_->err") >= 2);
    CHECK(privacy_src.find("Color(current_palette_->warn") != std::string::npos);
    CHECK(about_src.find("Color(current_palette_->warn") != std::string::npos);
    CHECK(privacy_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(privacy_src.find("Color(kLightPalette.warn") == std::string::npos);
    CHECK(about_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(about_src.find("Color(kLightPalette.warn") == std::string::npos);
    // Debug-log warning gated on the debug key, copy verbatim.
    CHECK(privacy_src.find("settings_.GetBool(\"debug_log_transcripts\")") != std::string::npos);
    CHECK(privacy_src.find("WARNING: dictated text will be written to the local log.") !=
          std::string::npos);
    // Retention ints keep exact ranges + formats (quoted literals intact).
    CHECK(privacy_src.find("IntRow(\"history_max_entries\", \"##history_max_entries\", 10, 100000, "
                           "\"keep %d entries\")") != std::string::npos);
    CHECK(privacy_src.find("IntRow(\"history_max_age_days\", \"##history_max_age_days\", 1, 3650, "
                           "\"delete after %d days\")") != std::string::npos);
    // Schema-key literals preserved on Privacy (schema-key test carve-outs
    // unchanged; NO schema change).
    CHECK(privacy_src.find("GetBool(\"history_enabled\")") != std::string::npos);
    CHECK(privacy_src.find("GetBool(\"keep_raw_in_history\")") != std::string::npos);
    CHECK(privacy_src.find("GetBool(\"keep_usage_stats\")") != std::string::npos);
    CHECK(privacy_src.find("GetString(\"llm_provider\")") != std::string::npos);
    CHECK(privacy_src.find("GetString(\"openrouter_model\")") != std::string::npos);
    CHECK(privacy_src.find("No telemetry.") != std::string::npos);
    // OpenRouter readout: warn copy verbatim, model read-only.
    CHECK(privacy_src.find("Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is "
                           "on.") != std::string::npos);
    CHECK(privacy_src.find("settings_.GetString(\"llm_provider\") == \"openrouter\"") !=
          std::string::npos);
    CHECK(privacy_src.find("\"not set\"") != std::string::npos);
    // Delete block: copy verbatim + call site + Esc upgrade (api-key
    // precedent: Esc cancels, focus lands on Cancel).
    CHECK(privacy_src.find("\"Delete all local data\"") != std::string::npos);
    CHECK(privacy_src.find("This cannot be undone. The app keeps running.") != std::string::npos);
    CHECK(privacy_src.find("\"Confirm delete\"") != std::string::npos);
    CHECK(privacy_src.find("\"Cancel\"") != std::string::npos);
    CHECK(privacy_src.find("DeleteLocalData()") != std::string::npos);
    CHECK(privacy_src.find("ImGui::IsKeyPressed(ImGuiKey_Escape)") != std::string::npos);
    CHECK(privacy_src.find("ImGui::SetKeyboardFocusHere()") != std::string::npos);
    // Code needles, not the file-scope comment that names the flag.
    CHECK(privacy_src.find("static bool focus_cancel_pending_ = false;") != std::string::npos);
    CHECK(privacy_src.find("focus_cancel_pending_ = true;") != std::string::npos);
    CHECK(privacy_src.find("BLOCK EXCEPTION") != std::string::npos);
    // DeleteLocalData body untouched: files-then-stores, config.json never
    // removed (span-scoped to its own body).
    const std::size_t del_fn = content.find("void MainWindow::DeleteLocalData");
    REQUIRE(del_fn != std::string::npos);
    const std::size_t del_clear = content.find("void MainWindow::ClearApiKeyInputs", del_fn);
    REQUIRE(del_clear != std::string::npos);
    const std::string del_src = content.substr(del_fn, del_clear - del_fn);
    CHECK(del_src.find("remove(base / \"api_keys.json\"") != std::string::npos);
    CHECK(del_src.find("config.json") == std::string::npos);
    CHECK(del_src.find("delete_data_armed_ = false;") != std::string::npos);
    // Tab switch clears the delete arm (safer destructive UX), close to
    // the ClearApiKeyInputs call it sits next to.
    const std::size_t settings_page = content.find("void MainWindow::DrawSettingsPage");
    REQUIRE(settings_page != std::string::npos);
    const std::size_t tab_switch = content.find("ClearApiKeyInputs();", settings_page);
    REQUIRE(tab_switch != std::string::npos);
    const std::size_t tab_arm_clear = content.find("delete_data_armed_ = false;", tab_switch);
    REQUIRE(tab_arm_clear != std::string::npos);
    CHECK(tab_arm_clear - tab_switch < 200);
    // About is read-only: version + config version + OpenRouter warn.
    CHECK(about_src.find("lwf::AppVersion()") != std::string::npos);
    CHECK(about_src.find("GetInt(\"schema_version\")") != std::string::npos);
    CHECK(about_src.find("GetString(\"llm_provider\")") != std::string::npos);
    CHECK(about_src.find("Cleanup uses OpenRouter: text is sent to OpenRouter when cleanup is "
                         "on.") != std::string::npos);
    CHECK(about_src.find("Local-first voice dictation.") != std::string::npos);
    // Shortcuts table (NEW, read-only): the 5 General labels, stored
    // values via GetString, empty shows "Not set".
    CHECK(about_src.find("\"Push-to-talk\"") != std::string::npos);
    CHECK(about_src.find("\"Hands-free\"") != std::string::npos);
    CHECK(about_src.find("\"Cancel\"") != std::string::npos);
    CHECK(about_src.find("\"Command mode\"") != std::string::npos);
    CHECK(about_src.find("\"Paste last\"") != std::string::npos);
    CHECK(about_src.find("settings_.GetString(shortcut.key)") != std::string::npos);
    CHECK(about_src.find("\"Not set\"") != std::string::npos);
    CHECK(about_src.find("\"hotkey_ptt\"") != std::string::npos);
    CHECK(about_src.find("\"hotkey_handsfree\"") != std::string::npos);
    CHECK(about_src.find("\"hotkey_cancel\"") != std::string::npos);
    CHECK(about_src.find("\"hotkey_command\"") != std::string::npos);
    CHECK(about_src.find("\"hotkey_paste_last\"") != std::string::npos);
    // Zero writes anywhere on About (span-scoped absence).
    CHECK(about_src.find("SetString(") == std::string::npos);
    CHECK(about_src.find("SetBool(") == std::string::npos);
    CHECK(about_src.find("SetInt(") == std::string::npos);
    CHECK(about_src.find("ApplyAndSave()") == std::string::npos);
}

TEST_CASE("T8 Home ruled rows pin") {
    // T8-owned pins: Home uses ruled rows (widgets::Row +
    // widgets::ToggleSwitch with the live palette), keeps or carries its
    // five former card ids (first Row id or PushID for the uncarded
    // blocks), keeps every write path verbatim (dictation double-write,
    // Learn empty payload, Copied dwell), keeps the Trust gating
    // divergence (no cleanup_enabled gate on Home), and reads every color
    // from the live palette (F1 err/warn absence). Span-scoped
    // DrawHomePage..DrawSpeechPage like the T5/T6/T7 cases so the General
    // hotkey row's StartLearnPayload cannot satisfy the Learn pin.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    const std::size_t home = content.find("void MainWindow::DrawHomePage");
    REQUIRE(home != std::string::npos);
    const std::size_t speech = content.find("void MainWindow::DrawSpeechPage", home);
    REQUIRE(speech != std::string::npos);
    const std::string home_src = content.substr(home, speech - home);
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // Ruled rows with the live palette (never the Paper default).
    CHECK(home_src.find("widgets::Row(") != std::string::npos);
    CHECK(home_src.find("widgets::ToggleSwitch(") != std::string::npos);
    CHECK(home_src.find("*current_palette_") != std::string::npos);
    // T8 toggles use the widget switch directly (local ToggleRow/BoolRow stay).
    CHECK(home_src.find("ToggleRow(") == std::string::npos);
    CHECK(home_src.find("BoolRow(") == std::string::npos);
    // Home no longer calls BeginCard (the def stays for later pages).
    CHECK(home_src.find("BeginCard(") == std::string::npos);
    CHECK(content.find("void MainWindow::BeginCard") != std::string::npos);
    // Former card ids: rows carry them directly; the Trust and Last blocks
    // carry theirs via real PushID (a comment alone must not pass).
    CHECK(home_src.find("\"##card_now\"") != std::string::npos);
    CHECK(home_src.find("\"##card_hotkey\"") != std::string::npos);
    CHECK(home_src.find("ImGui::PushID(\"##card_trust\")") != std::string::npos);
    CHECK(home_src.find("\"##card_quick\"") != std::string::npos);
    CHECK(home_src.find("ImGui::PushID(\"##card_last\")") != std::string::npos);
    // Hotkey compound slot is 360 design px (General precedent), toggles 52.
    CHECK(home_src.find("360.0f * scale") != std::string::npos);
    CHECK(home_src.find("52.0f * scale") != std::string::npos);
    // Headings carry the 16 px indent + card-title font (T5/T6/T7 pattern).
    CHECK(home_src.find("SetCursorPosX(16.0f * scale)") != std::string::npos);
    CHECK(home_src.find("PushFont(nullptr, metrics::font_card_title_px)") != std::string::npos);
    CHECK(home_src.find("TextUnformatted(\"Right now\")") != std::string::npos);
    CHECK(home_src.find("TextUnformatted(\"Trust\")") != std::string::npos);
    // F1: every color in the span reads the live palette (status colors +
    // Trust warn included); no Paper read of any token remains.
    CHECK(home_src.find("current_palette_->ok") != std::string::npos);
    CHECK(home_src.find("current_palette_->muted") != std::string::npos);
    CHECK(home_src.find("current_palette_->accent") != std::string::npos);
    CHECK(home_src.find("current_palette_->text") != std::string::npos);
    CHECK(home_src.find("current_palette_->warn") != std::string::npos);
    CHECK(home_src.find("kLightPalette") == std::string::npos);
    // Hot-apply: no restart flag and no restart banner anywhere on Home
    // (every Home key is hot-apply; nothing restart-gated lands here).
    CHECK(home_src.find("restart_needed_ = true") == std::string::npos);
    CHECK(home_src.find("DrawRestartBanner(") == std::string::npos);
    // Right-now FSM mapping, copy verbatim.
    CHECK(home_src.find("status_word = \"Paused\"") != std::string::npos);
    CHECK(home_src.find("\"Dictation is off.\"") != std::string::npos);
    CHECK(home_src.find("status_word = \"Listening\"") != std::string::npos);
    CHECK(home_src.find("\"Speak, then release.\"") != std::string::npos);
    CHECK(home_src.find("status_word = \"Thinking\"") != std::string::npos);
    CHECK(home_src.find("\"Turning speech into text.\"") != std::string::npos);
    CHECK(home_src.find("\"Set a hotkey to start.\"") != std::string::npos);
    CHECK(home_src.find("\"Hold the hotkey to dictate.\"") != std::string::npos);
    CHECK(home_src.find("State::CAPTURING") != std::string::npos);
    CHECK(home_src.find("State::TRANSCRIBING") != std::string::npos);
    CHECK(home_src.find("State::POSTPROCESSING") != std::string::npos);
    CHECK(home_src.find("State::EMITTING") != std::string::npos);
    // Learn: posts StartLearnHotkey with an EMPTY payload (Home span only —
    // the StartLearnPayload overload exists on General, outside this span).
    CHECK(home_src.find("bus_.PostCommand(UiCommandType::StartLearnHotkey);") != std::string::npos);
    CHECK(home_src.find("StartLearnPayload") == std::string::npos);
    CHECK(home_src.find("\"Learn a new chord...\"") != std::string::npos);
    CHECK(home_src.find("DrawHotkeyChips(settings_.GetString(\"hotkey_ptt\"), scale)") !=
          std::string::npos);
    // Trust: derivation + copy verbatim, and the gating divergence (Home
    // does NOT read cleanup_enabled here; the strip does).
    CHECK(home_src.find("settings_.GetString(\"stt_provider\")") != std::string::npos);
    CHECK(home_src.find("settings_.GetBool(\"stt_consent_accepted\")") != std::string::npos);
    CHECK(home_src.find("settings_.GetString(\"llm_provider\") == \"openrouter\"") !=
          std::string::npos);
    {
        const std::size_t trust = home_src.find("ImGui::PushID(\"##card_trust\")");
        REQUIRE(trust != std::string::npos);
        const std::size_t trust_end = home_src.find("ImGui::PopID();", trust);
        REQUIRE(trust_end != std::string::npos);
        const std::string trust_src = home_src.substr(trust, trust_end - trust);
        CHECK(trust_src.find("cleanup_enabled") == std::string::npos);
        CHECK(trust_src.find("\"Speech is sent to %s for each sentence.\"") != std::string::npos);
        CHECK(trust_src.find("\"Cleanup uses OpenRouter: text is sent to OpenRouter when "
                             "cleanup is on.\"") != std::string::npos);
        CHECK(trust_src.find("\"Everything runs on this computer.\"") != std::string::npos);
        CHECK(trust_src.find("\"Cloud speech and OpenRouter cleanup are off.\"") !=
              std::string::npos);
    }
    // Quick settings: ##<key> toggle ids + the dictation double-write
    // (SetBool + SetEnabled + ApplyAndSave, in that order, close enough
    // that a later card cannot supply the pin).
    CHECK(home_src.find("widgets::ToggleSwitch(\"##dictation_enabled\"") != std::string::npos);
    CHECK(home_src.find("widgets::ToggleSwitch(\"##cleanup_enabled\"") != std::string::npos);
    CHECK(home_src.find("widgets::ToggleSwitch(\"##double_tap_handsfree\"") != std::string::npos);
    CHECK(home_src.find("SetBool(\"dictation_enabled\", !dictation)") != std::string::npos);
    CHECK(home_src.find("bus_.PostCommand(UiCommandType::SetEnabled, !dictation ? \"1\" : \"0\")") !=
          std::string::npos);
    CHECK(home_src.find("SetBool(\"cleanup_enabled\", !cleanup)") != std::string::npos);
    CHECK(home_src.find("SetBool(\"double_tap_handsfree\", !hands_free)") != std::string::npos);
    CHECK(count_occurrences(home_src, "ApplyAndSave()") == 3);
    // Last dictation: Copied dwell + clipboard path, copy verbatim.
    CHECK(home_src.find("copied_until_") != std::string::npos);
    CHECK(home_src.find("\"Copied\"") != std::string::npos);
    CHECK(home_src.find("ImGui::SetClipboardText(last_transcript_.c_str())") != std::string::npos);
    CHECK(home_src.find("metrics::saved_dwell") != std::string::npos);
    CHECK(home_src.find("\"Nothing yet.\"") != std::string::npos);
    // Block exception literal (T6/T7 pin rule).
    CHECK(count_occurrences(home_src, "BLOCK EXCEPTION") >= 2);
    // DrawHotkeyChips internals (shared with General) read the live palette.
    const std::size_t chips = content.find("void MainWindow::DrawHotkeyChips");
    REQUIRE(chips != std::string::npos);
    const std::size_t chips_end = content.find("void MainWindow::DrawGeneralHotkeyRow", chips);
    REQUIRE(chips_end != std::string::npos);
    const std::string chips_src = content.substr(chips, chips_end - chips);
    CHECK(chips_src.find("kLightPalette") == std::string::npos);
    CHECK(chips_src.find("current_palette_->text") != std::string::npos);
    CHECK(chips_src.find("current_palette_->border") != std::string::npos);
    CHECK(chips_src.find("current_palette_->muted") != std::string::npos);
}

TEST_CASE("T9 Welcome onboarding ruled rows and step-3 download pin") {
    // T9-owned pins: Welcome uses ruled rows (widgets::Row + RowEnd with the
    // live palette) plus documented uncarded blocks for the mic meter and
    // the catalog listing (both taller than the 52 px ruled row, T6/T7
    // precedent). The subtitle mismatch is fixed (Four, not Three), the
    // restart banner is drawn (step 3 sets the flag via the model edit and
    // the shared download latch), every color reads the live palette, and
    // step 3 wires Download on the ONE shared worker with the Speech tab
    // (same catalog rule, same spawn gate, same latch in RunOnce — no
    // second worker, no Speech id reuse, so the T13a Speech pins stay
    // green). Step-2 history is untouched: the learn flow lives in
    // SetLearnedHotkey (the dead SetDetectedHotkey API was removed with the
    // retired wizard, followups D7) and the Get-started focus arm stays the
    // only Welcome page_focus_pending_ site. Span-scoped
    // DrawWelcomePage..EOF (Welcome is the last function in the file) so
    // lookalike strings elsewhere cannot satisfy these pins by accident.
    const std::string content = ReadSource("src/ui/MainWindow.cpp");
    REQUIRE_FALSE(content.empty());
    const std::size_t welcome = content.find("void MainWindow::DrawWelcomePage");
    REQUIRE(welcome != std::string::npos);
    const std::string welcome_src = content.substr(welcome);
    const auto count_occurrences = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };
    // Followups D7: the four-step test wizard is retired. The page keeps
    // the model machinery and gains a one-action Get started; the step
    // labels and the wizard-only copy must not come back.
    CHECK(welcome_src.find("\"Dictation from this computer, in one hotkey.\"") !=
          std::string::npos);
    CHECK(welcome_src.find("Four small steps") == std::string::npos);
    CHECK(welcome_src.find("Step 1 of 4") == std::string::npos);
    CHECK(welcome_src.find("Step 2 of 4") == std::string::npos);
    CHECK(welcome_src.find("Step 3 of 4") == std::string::npos);
    CHECK(welcome_src.find("Step 4 of 4") == std::string::npos);
    CHECK(welcome_src.find("Speak now; the bar should move.") == std::string::npos);
    CHECK(welcome_src.find("Waiting for a chord...") == std::string::npos);
    CHECK(welcome_src.find("Skip (I have models)") == std::string::npos);
    CHECK(welcome_src.find("Hold the push-to-talk hotkey, speak, and release.") !=
          std::string::npos);
    CHECK(welcome_src.find("Get started") != std::string::npos);
    CHECK(welcome_src.find("unverified file") != std::string::npos);
    CHECK(welcome_src.find("Files are verified against the listed SHA-256 checksums.") !=
          std::string::npos);
    // Ruled rows with the live palette; the two tall sections stay blocks.
    CHECK(welcome_src.find("widgets::Row(") != std::string::npos);
    CHECK(welcome_src.find("widgets::RowEnd(") != std::string::npos);
    CHECK(welcome_src.find("*current_palette_") != std::string::npos);
    CHECK(count_occurrences(welcome_src, "widgets::Row(") >= 3);
    CHECK(count_occurrences(welcome_src, "BLOCK EXCEPTION") >= 2);
    // Headings carry the 16 px indent + card-title font (T5/T6/T7 pattern).
    CHECK(welcome_src.find("SetCursorPosX(16.0f * scale)") != std::string::npos);
    CHECK(welcome_src.find("PushFont(nullptr, metrics::font_card_title_px)") != std::string::npos);
    // Welcome no longer calls BeginCard (the def stays for its own pins).
    CHECK(welcome_src.find("BeginCard(") == std::string::npos);
    CHECK(content.find("void MainWindow::BeginCard") != std::string::npos);
    // Former card id carried by a real PushID (a comment alone must not pass).
    CHECK(welcome_src.find("ImGui::PushID(\"##card_welcome\")") != std::string::npos);
    CHECK(welcome_src.find("ImGui::PushID(\"##card_welcome_models\")") != std::string::npos);
    // Restart banner: drawn once, after the page chrome (step-3 edits and
    // download success set the flag; the RunOnce latch surfaces it here).
    CHECK(count_occurrences(welcome_src, "DrawRestartBanner(scale)") == 1);
    // F1 err/warn in the live palette, Paper variant pinned out of the span.
    CHECK(count_occurrences(welcome_src, "Color(current_palette_->warn") >= 2);
    CHECK(count_occurrences(welcome_src, "Color(current_palette_->err") >= 1);
    CHECK(welcome_src.find("Color(kLightPalette.err") == std::string::npos);
    CHECK(welcome_src.find("Color(kLightPalette.warn") == std::string::npos);
    // Step-3 download: Welcome ids, never the Speech ids (same window would
    // collide), same availability rule and spawn gate, shared latch.
    CHECK(welcome_src.find("\"Download##download_welcome\"") != std::string::npos);
    CHECK(welcome_src.find("\"Cancel##cancel_download_welcome\"") != std::string::npos);
    CHECK(welcome_src.find("\"Download##download_speech\"") == std::string::npos);
    CHECK(welcome_src.find("\"Cancel##cancel_download\"") == std::string::npos);
    CHECK(welcome_src.find("ParseModelsJson") != std::string::npos);
    CHECK(welcome_src.find("IsSafeFileName") != std::string::npos);
    CHECK(welcome_src.find("PlanDownloadStart") != std::string::npos);
    CHECK(welcome_src.find("BeginDisabled(") != std::string::npos);
    CHECK(welcome_src.find("download_pressed && download_available && !download_busy_.load()") !=
          std::string::npos);
    CHECK(welcome_src.find("download_cancelled_.store(true)") != std::string::npos);
    CHECK(welcome_src.find("download_model_.Snapshot()") != std::string::npos);
    CHECK(welcome_src.find("Download complete. Restart the app to use the new model.") !=
          std::string::npos);
    CHECK(welcome_src.find("No download address is listed for this file yet.") != std::string::npos);
    // Browse (and therefore the model-path edit) stays enabled during a
    // transfer, mirroring Speech — the download disable must wrap only the
    // Download button, never Browse.
    {
        const std::size_t browse = welcome_src.find("\"Browse...##browse_welcome\"");
        REQUIRE(browse != std::string::npos);
        const std::size_t disable = welcome_src.find("BeginDisabled(", browse);
        REQUIRE(disable != std::string::npos);
        CHECK(disable > browse);
    }
    // One shared download state struct, one worker thread (header) — the
    // mirror-not-extract shape, no second worker for Welcome.
    const std::string header = ReadSource("src/ui/MainWindow.h");
    REQUIRE_FALSE(header.empty());
    CHECK(count_occurrences(header, "Onboarding::DownloadModel download_model_") == 1);
    CHECK(count_occurrences(header, "std::thread download_worker_") == 1);
    // Step-2 history: no StartLearnHotkey inside Welcome. The chord-detect
    // API (SetDetectedHotkey + detected_hotkey_) was dead once the wizard's
    // step 2 was retired (followups D7) and is removed; SetLearnedHotkey
    // stays for the Home/Settings learn flows.
    CHECK(welcome_src.find("StartLearnHotkey") == std::string::npos);
    CHECK(welcome_src.find("SetDetectedHotkey") == std::string::npos);
    CHECK(content.find("void MainWindow::SetDetectedHotkey") == std::string::npos);
    CHECK(content.find("detected_hotkey_") == std::string::npos);
    const std::size_t learned = content.find("void MainWindow::SetLearnedHotkey");
    REQUIRE(learned != std::string::npos);
    const std::string learned_rest =
        content.substr(learned, content.find("void MainWindow::SetRecorderError", learned) - learned);
    CHECK(learned_rest.find("target_key == \"hotkey_handsfree\"") != std::string::npos);
    // Finish semantics byte-identical: first_run off + save + single focus
    // arm (the global double-arm invariant at the strip stays green).
    CHECK(count_occurrences(content, "page_focus_pending_ = true") == 2);
    CHECK(count_occurrences(welcome_src, "page_focus_pending_ = true") == 1);
    CHECK(welcome_src.find("SetBool(\"first_run\", false)") != std::string::npos);
    CHECK(welcome_src.find("GetString(\"hotkey_ptt\"") != std::string::npos);
    CHECK(welcome_src.find("SetString(\"whisper_model_path\"") != std::string::npos);
    // Save count preserved (step-2 accept + Finish, plus the download
    // skip path mirroring Speech).
    CHECK(count_occurrences(welcome_src, "ApplyAndSave()") >= 2);
    // Speech ids stay in the Speech span (collision guard reads the whole
    // file so a rename-everywhere refactor cannot pass silently).
    CHECK(content.find("\"Download##download_speech\"") != std::string::npos);
    CHECK(content.find("\"Cancel##cancel_download\"") != std::string::npos);
}
