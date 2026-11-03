#include "core/Languages.h"
#include "core/OverlayModel.h"
#include "core/SettingsStore.h"
#include "core/SettingsUiModel.h"
#include "ui/theme/ThemePalette.h"

#include <doctest/doctest.h>
#include <cmath>
#include <fstream>
#include <string>

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

TEST_CASE("settings diff classifies hot apply versus restart required") {
    const std::string path = "/tmp/lwf_diff_settings.json";
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
